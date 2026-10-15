#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

// The parts of the DXBC container and program format the shader fixes read.
namespace omvr::core::render {

// One operand: its token position and the register index (1D operands).
struct Operand {
    std::size_t at;      // operand token
    std::size_t size;    // tokens including extensions, indices, immediates
    std::uint32_t type;
    std::uint32_t reg;   // index 0 for 1D operands
};

// Parses the operand at `t[i]`; only the simple forms these shaders use.
std::optional<Operand> parse_operand(const std::uint32_t* t, std::size_t end, std::size_t i);

// Whether operand `o` is the 4-component immediate (x, y, z, w).
bool is_immediate4(const std::uint32_t* t, const Operand& o, std::uint32_t x, std::uint32_t y,
                   std::uint32_t z, std::uint32_t w);

// DXBC checksum (MD5 compression with DXBC's own final block) of a blob;
// it is stored at bytes 4..19 of the container.
void dxbc_checksum(const std::uint8_t* blob, std::size_t total, std::uint32_t out[4]) noexcept;

} // namespace omvr::core::render
