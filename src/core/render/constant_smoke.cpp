// OpenMotherVR-Provenance: clean
#include "core/render/constant_smoke.h"

#include "core/render/dxbc.h"
#include "core/render/dxbc_tokens.h"
#include "core/render/tuning.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <span>
#include <string_view>

namespace omvr::core::render {

namespace {

// 24 §15.6: at install, the brightness in use.
constexpr std::string_view kOnLog = "proxy: Constant smoke lighting on, brightness {:.2f}";

// ---- The resource binding (RDEF) -------------------------------------------------

// The RDEF chunk's header: the binding count and offset (words 2 and 3) and
// the version word (minor in bits 0..7, major in 8..15).
constexpr std::size_t kRdefHeaderBytes = 28;
constexpr std::size_t kRdefBindCount   = 2 * kTokenBytes;
constexpr std::size_t kRdefBindOffset  = 3 * kTokenBytes;
constexpr std::size_t kRdefVersion     = 4 * kTokenBytes;
constexpr std::uint32_t kVersionByte   = 0xFF;
constexpr std::uint32_t kMajorShift    = 8;
constexpr std::string_view kRdefChunk  = "RDEF";
// A binding: name offset, input type, return type, dimension, sample count,
// bind point, bind count, flags; shader model 5.1 adds a space and an id.
constexpr std::size_t kBindingBytes   = 8 * kTokenBytes;
constexpr std::size_t kBindingBytes51 = 10 * kTokenBytes;
constexpr std::size_t kBindingType    = 1 * kTokenBytes;
constexpr std::size_t kBindingPoint   = 5 * kTokenBytes;
constexpr std::size_t kBindingCount   = 6 * kTokenBytes;
constexpr std::uint32_t kInputTexture = 2; // D3D_SIT_TEXTURE

// The pre-filter: the atlas's name and its terminating zero, as the
// binding's name lies in the blob.
consteval std::array<std::uint8_t, kAlphaLightResourceName.size() + 1> atlas_name() {
    std::array<std::uint8_t, kAlphaLightResourceName.size() + 1> bytes{};
    for (std::size_t i = 0; i < kAlphaLightResourceName.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(kAlphaLightResourceName[i]);
    return bytes;
}
constexpr auto kAtlasName = atlas_name();

std::uint32_t u32_at(std::span<const std::uint8_t> b, std::size_t at) {
    std::uint32_t v = 0;
    std::memcpy(&v, b.data() + at, sizeof(v));
    return v;
}

// The null-terminated name at `at`, or empty when it runs past the chunk.
std::string_view name_at(std::span<const std::uint8_t> chunk, std::size_t at) {
    if (at >= chunk.size()) return {};
    const auto* begin = reinterpret_cast<const char*>(chunk.data() + at);
    const auto* end   = static_cast<const char*>(std::memchr(begin, 0, chunk.size() - at));
    return end ? std::string_view(begin, static_cast<std::size_t>(end - begin)) : std::string_view{};
}

// The container binds the atlas by its name as one texture at t9 (24 §15.2,
// 08 §8.1).
bool binds_atlas(std::span<const std::uint8_t> in) {
    const auto rdef = find_chunk(in, kRdefChunk);
    if (!rdef || rdef->size() < kRdefHeaderBytes) return false;
    const std::uint32_t version = u32_at(*rdef, kRdefVersion);
    const std::uint32_t major   = (version >> kMajorShift) & kVersionByte;
    const std::uint32_t minor   = version & kVersionByte;
    const std::size_t stride    = (major > 5 || (major == 5 && minor >= 1)) ? kBindingBytes51 : kBindingBytes;
    const std::uint32_t count   = u32_at(*rdef, kRdefBindCount);
    const std::uint32_t offset  = u32_at(*rdef, kRdefBindOffset);
    if (offset > rdef->size() || count > (rdef->size() - offset) / stride) return false;
    bool found = false;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t at = offset + std::size_t(i) * stride;
        if (name_at(*rdef, u32_at(*rdef, at)) != kAlphaLightResourceName) continue;
        if (found || u32_at(*rdef, at + kBindingType) != kInputTexture ||
            u32_at(*rdef, at + kBindingPoint) != kAlphaLightSlot || u32_at(*rdef, at + kBindingCount) != 1)
            return false;
        found = true;
    }
    return found;
}

// ---- The program -----------------------------------------------------------------

bool is_declaration(std::uint32_t op) {
    return (op >= kOpFirstSm4Declaration && op <= kOpLastSm4Declaration) ||
           (op >= kOpFirstSm5Declaration && op <= kOpLastSm5Declaration) || op == kOpDclGsInstanceCount;
}

// t[p] and t[p + 1] are a texture operand of slot t9 with an immediate index.
bool is_atlas_operand(const std::vector<std::uint32_t>& t, std::size_t p) {
    const std::uint32_t tok = t[p];
    return op_type(tok) == kTypeResource && index_dim(tok) == kIndex1D &&
           index_rep(tok, 0) == kIndexImmediate32 && t[p + 1] == kAlphaLightSlot;
}

std::uint32_t mask_of(std::uint32_t tok) { return (tok >> kComponentShift) & kWriteMaskBits; }
std::uint32_t swizzle_of(std::uint32_t tok) { return (tok >> kComponentShift) & 0xFF; }

constexpr std::uint32_t kMaskXyz = 0x7, kMaskYzw = 0xE;

// The sample of 24 §15.2: `sample[_indexable(texture2d)] rN.xyz, coord,
// t9.xyzw, sM` or the same into rN.yzw from t9.wxyz; no saturate, no
// offsets, a plain temporary as destination. Returns the destination's
// operand token index.
std::optional<std::size_t> match_sample(const std::vector<std::uint32_t>& t, std::size_t i, std::size_t len) {
    const std::uint32_t tok = t[i];
    if (opcode(tok) != kOpSample || (tok & kSaturateFlag) != 0) return std::nullopt;
    const std::size_t end = i + len;
    std::size_t p         = i + 1;
    if (op_extended(tok)) {
        // The resource's dimension and return type only (no offsets).
        bool more = true;
        while (more) {
            if (p >= end) return std::nullopt;
            const std::uint32_t ext  = t[p++];
            const std::uint32_t type = ext & kExtendedTypeMask;
            if (type == kExtendedResourceDim) {
                if (((ext >> kExtendedDimShift) & kExtendedDimMask) != kResourceDimTexture2D)
                    return std::nullopt;
            } else if (type != kExtendedResourceReturn) {
                return std::nullopt;
            }
            more = op_extended(ext);
        }
    }
    const auto dst = parse_operand(t.data(), end, p);
    if (!dst) return std::nullopt;
    const auto coord = parse_operand(t.data(), end, dst->at + dst->size);
    if (!coord) return std::nullopt;
    const auto res = parse_operand(t.data(), end, coord->at + coord->size);
    if (!res) return std::nullopt;
    const auto smp = parse_operand(t.data(), end, res->at + res->size);
    if (!smp || smp->at + smp->size != end) return std::nullopt;

    const std::uint32_t d = t[dst->at];
    if (dst->type != kTypeTemp || dst->size != 2 || op_extended(d) || index_dim(d) != kIndex1D ||
        (d & kComponentCountMask) != kFourComponents || selection_mode(d) != kSelectMask)
        return std::nullopt;
    const std::uint32_t r = t[res->at];
    if (res->size != 2 || op_extended(r) || !is_atlas_operand(t, res->at) ||
        (r & kComponentCountMask) != kFourComponents || selection_mode(r) != kSelectSwizzle)
        return std::nullopt;
    if (smp->type != kTypeSampler || smp->size != 2) return std::nullopt;
    const bool xyz = mask_of(d) == kMaskXyz && swizzle_of(r) == swizzle(kX, kY, kZ, kW);
    const bool yzw = mask_of(d) == kMaskYzw && swizzle_of(r) == swizzle(kW, kX, kY, kZ);
    if (!xyz && !yzw) return std::nullopt;
    return dst->at;
}

// The one atlas sample of the program: its instruction (token index and
// length) and destination, or nothing when the atlas is read other than by
// exactly one sample of 24 §15.2.
struct Sample {
    std::size_t at  = 0;
    std::size_t len = 0;
    std::size_t dst = 0;
};

std::optional<Sample> find_sample(const std::vector<std::uint32_t>& t, std::size_t end) {
    const auto insts = instructions(t, end);
    if (!insts) return std::nullopt;
    std::optional<Sample> found;
    for (const Instruction& in : *insts) {
        if (in.op == kOpCustomData || is_declaration(in.op)) continue;
        bool reads_atlas = false;
        for (std::size_t p = in.at + 1; p + 1 < in.at + in.len; ++p)
            reads_atlas = reads_atlas || is_atlas_operand(t, p);
        if (!reads_atlas) continue;
        const auto dst = match_sample(t, in.at, in.len);
        if (found || !dst) return std::nullopt;
        found = Sample{in.at, in.len, *dst};
    }
    return found;
}

// mov: opcode, the destination (2 tokens) and a 4-component immediate (5).
constexpr std::uint32_t kMovLength = 8;

std::vector<std::uint32_t> rewrite(const std::vector<std::uint32_t>& t, const Sample& s, float brightness) {
    const std::uint32_t b = std::bit_cast<std::uint32_t>(brightness);
    std::vector<std::uint32_t> prog;
    prog.reserve(t.size());
    prog.insert(prog.end(), t.begin(), t.begin() + static_cast<std::ptrdiff_t>(s.at));
    prog.push_back(opcode_token(kOpMov, kMovLength));
    prog.push_back(t[s.dst]);
    prog.push_back(t[s.dst + 1]);
    prog.insert(prog.end(), {kFourComponents | (kTypeImmediate32 << kTypeShift), b, b, b, b});
    prog.insert(prog.end(), t.begin() + static_cast<std::ptrdiff_t>(s.at + s.len), t.end());
    prog[1] = static_cast<std::uint32_t>(prog.size() - (t.size() - t[1]));
    return prog;
}

std::vector<std::uint8_t> patch(std::span<const std::uint8_t> in, float brightness) {
    if (in.size() < kContainerHeaderSize || std::memcmp(in.data(), kDxbcMagic.data(), kDxbcMagicBytes) != 0)
        return {};
    // Cheap pre-filter (24 §15.4): the atlas's name, terminated, as the
    // resource binding holds it; most shaders do not have it, and skipping
    // them avoids a checksum of each.
    if (std::ranges::search(in, kAtlasName).empty()) return {};

    const auto chunk = find_program(in);
    if (!chunk || !binds_atlas(in)) return {};
    const std::vector<std::uint32_t> t = program_tokens(in, *chunk);
    if (((t[0] >> kProgramTypeShift) & kProgramTypeMask) != kPixelShaderProgram) return {};
    if (t[1] < kProgramHeaderTokens || t[1] > t.size()) return {};

    const auto sample = find_sample(t, t[1]);
    if (!sample) return {};
    return reassemble(in, *chunk, rewrite(t, *sample, brightness));
}

} // namespace

std::vector<std::uint8_t> patch_constant_smoke(const void* blob, std::size_t size,
                                               float brightness) noexcept {
    try {
        return patch({static_cast<const std::uint8_t*>(blob), size}, brightness);
    } catch (...) {
        // An allocation failed: the original shader is used.
        return {};
    }
}

void ConstantSmoke::start(bool on, float brightness) noexcept {
    if (!on) return;
    brightness_.store(brightness, std::memory_order_relaxed);
    on_.store(true, std::memory_order_release);
    diag::ports::logf(log_, kOnLog, brightness);
}

std::optional<float> ConstantSmoke::brightness() const noexcept {
    if (!on_.load(std::memory_order_acquire)) return std::nullopt;
    return brightness_.load(std::memory_order_relaxed);
}

} // namespace omvr::core::render
