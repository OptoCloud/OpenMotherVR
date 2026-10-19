// OpenMotherVR-Provenance: clean
#pragma once

// A small interpreter of Shader Model 5 programs for the shader-fix tests,
// written from the public token format (d3d11TokenizedProgramFormat.hpp)
// independently of the patchers in core/render, so the tests check what a
// rewritten program computes. It runs the arithmetic the game's shaders
// and the fixes use: mov, add, mul, mad, max, dp2, dp4, frc, sqrt, rsq, lt
// and movc, on temporaries, inputs, immediates (one or four components)
// and constant buffers, with swizzles, select-1 sources, write masks and
// the negate and abs modifiers. Declarations are skipped; ret ends the run.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace omvr::tests {

using Reg4 = std::array<float, 4>;

struct Sm5Machine {
    std::array<Reg4, 8> r{};
    std::array<Reg4, 4> v{};
    // cb0 to cb2, 40 registers each.
    std::array<std::array<Reg4, 40>, 3> cb{};

    // Opcodes (the low 11 bits of an opcode token).
    static constexpr std::uint32_t kAdd = 0x00, kDp2 = 0x0F, kDp4 = 0x11, kFrc = 0x1A, kLt = 0x31,
                                   kMad = 0x32, kMax = 0x34, kMov = 0x36, kMovc = 0x37, kMul = 0x38,
                                   kRet = 0x3E, kRsq = 0x44, kSqrt = 0x4B, kFirstDcl = 0x58, kLastDcl = 0x6A;
    // Operand types.
    static constexpr std::uint32_t kTemp = 0, kInput = 1, kImmediate = 4, kConstantBuffer = 8;

    static std::uint32_t component(std::uint32_t tok, std::uint32_t c) { return (tok >> (4 + 2 * c)) & 3; }

    // Reads the source operand at t[p]; advances p past it.
    Reg4 source(const std::vector<std::uint32_t>& t, std::size_t& p) const {
        const std::uint32_t tok = t[p++];
        bool negate = false, absolute = false;
        if (tok & 0x80000000u) {
            const std::uint32_t ext = t[p++];
            REQUIRE((ext & 0x3F) == 1u);        // a modifier
            REQUIRE((ext & 0x80000000u) == 0u); // and only that
            const std::uint32_t modifier = (ext >> 6) & 0xFF;
            negate                       = modifier == 1 || modifier == 3;
            absolute                     = modifier == 2 || modifier == 3;
        }
        const std::uint32_t type = (tok >> 12) & 0xFF;
        Reg4 out{};
        if (type == kImmediate) {
            const std::uint32_t count = tok & 3;
            REQUIRE((count == 1 || count == 2)); // one component or four
            if (count == 2) {
                for (float& x : out) x = std::bit_cast<float>(t[p++]);
            } else {
                out.fill(std::bit_cast<float>(t[p++]));
            }
        } else {
            Reg4 reg{};
            if (type == kTemp) {
                reg = r.at(t[p++]);
            } else if (type == kInput) {
                reg = v.at(t[p++]);
            } else {
                REQUIRE(type == kConstantBuffer);
                reg = cb.at(t[p]).at(t[p + 1]);
                p += 2;
            }
            const std::uint32_t mode = (tok >> 2) & 3;
            REQUIRE((mode == 1 || mode == 2)); // a swizzle or one component
            for (std::uint32_t c = 0; c < 4; ++c) out[c] = reg[component(tok, mode == 2 ? 0 : c)];
        }
        for (float& x : out) {
            if (absolute) x = std::abs(x);
            if (negate) x = -x;
        }
        return out;
    }

    static std::size_t source_count(std::uint32_t op) {
        if (op == kMov || op == kSqrt || op == kRsq || op == kFrc) return 1;
        if (op == kMad || op == kMovc) return 3;
        return 2;
    }

    static float apply(std::uint32_t op, const std::array<Reg4, 3>& s, std::size_t k) {
        const Reg4 &a = s[0], &b = s[1], &c = s[2];
        switch (op) {
            case kMov: return a[k];
            case kAdd: return a[k] + b[k];
            case kMul: return a[k] * b[k];
            case kMad: return a[k] * b[k] + c[k];
            case kMax: return std::max(a[k], b[k]);
            case kDp2: return a[0] * b[0] + a[1] * b[1];
            case kDp4: return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
            case kFrc: return a[k] - std::floor(a[k]);
            case kSqrt: return std::sqrt(a[k]);
            case kRsq: return 1.0f / std::sqrt(a[k]);
            case kLt: return std::bit_cast<float>(a[k] < b[k] ? 0xFFFFFFFFu : 0u);
            case kMovc: return std::bit_cast<std::uint32_t>(a[k]) != 0 ? b[k] : c[k];
            default: FAIL("unexpected opcode " << op); return 0.0f;
        }
    }

    void run(const std::vector<std::uint32_t>& t) {
        for (std::size_t i = 2; i < t[1];) {
            const std::uint32_t op  = t[i] & 0x7FF;
            const std::uint32_t len = (t[i] >> 24) & 0x7F;
            REQUIRE(len > 0);
            if (op == kRet) return;
            if (op >= kFirstDcl && op <= kLastDcl) {
                i += len;
                continue;
            }
            REQUIRE((t[i] & 0x80000000u) == 0u); // no extended opcode
            std::size_t p           = i + 1;
            const std::uint32_t dst = t[p++];
            REQUIRE(((dst >> 12) & 0xFF) == kTemp);
            const std::uint32_t reg  = t[p++];
            const std::uint32_t mask = (dst >> 4) & 0xF;
            std::array<Reg4, 3> s{};
            for (std::size_t k = 0; k < source_count(op); ++k) s[k] = source(t, p);
            REQUIRE(p == i + len);
            Reg4 res{};
            for (std::size_t k = 0; k < 4; ++k) res[k] = apply(op, s, k);
            // Scalar results replicate; the mask picks the components.
            for (std::size_t k = 0; k < 4; ++k) {
                if (mask & (1u << k)) r.at(reg)[k] = res[k];
            }
            i += len;
        }
    }
};

} // namespace omvr::tests
