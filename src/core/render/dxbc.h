// OpenMotherVR-Provenance: clean
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// The parts of the DXBC container and program format the shader fixes read
// (08 §1). Pure.
namespace omvr::core::render {

// One operand: its token position and the register index (1D operands).
struct Operand {
    std::size_t at;   // operand token
    std::size_t size; // tokens including extensions, indices, immediates
    std::uint32_t type;
    std::uint32_t reg; // index 0 for 1D operands
};

// Parses the operand at `t[i]`, which must lie before `end`; only the simple
// forms these shaders use (immediate indices).
std::optional<Operand> parse_operand(const std::uint32_t* t, std::size_t end, std::size_t i);

// Whether operand `o` is the 4-component immediate (x, y, z, w).
bool is_immediate4(const std::uint32_t* t, const Operand& o, std::uint32_t x, std::uint32_t y,
                   std::uint32_t z, std::uint32_t w);

// The DXBC checksum of a whole container: MD5's compression with DXBC's own
// final block, over everything after the stored checksum and version. It is
// stored at bytes 4..19 of the container. `blob` holds at least the
// container header.
using DxbcChecksum = std::array<std::uint32_t, 4>;
DxbcChecksum dxbc_checksum(std::span<const std::uint8_t> blob) noexcept;

} // namespace omvr::core::render
