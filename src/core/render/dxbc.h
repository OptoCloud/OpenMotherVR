// OpenMotherVR-Provenance: clean
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

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

// One instruction of a program: where its opcode token lies, its length in
// tokens (a customData block's from the token after it), its opcode and
// whether its opcode token is extended.
struct Instruction {
    std::size_t at   = 0;
    std::size_t len  = 0;
    std::uint32_t op = 0;
    bool extended    = false;
};

// The instructions of the program `t` from after its header up to `end`
// (at most t.size()), in order, or nothing when one has no length or does
// not fit before `end`.
std::optional<std::vector<Instruction>> instructions(std::span<const std::uint32_t> t, std::size_t end);

// Whether operand `o` is the 4-component immediate (x, y, z, w).
bool is_immediate4(const std::uint32_t* t, const Operand& o, std::uint32_t x, std::uint32_t y,
                   std::uint32_t z, std::uint32_t w);

// The DXBC checksum of a whole container: MD5's compression with DXBC's own
// final block, over everything after the stored checksum and version. It is
// stored at bytes 4..19 of the container. `blob` holds at least the
// container header.
using DxbcChecksum = std::array<std::uint32_t, 4>;
DxbcChecksum dxbc_checksum(std::span<const std::uint8_t> blob) noexcept;

// The program chunk (SHEX or SHDR) of a container: where the chunk starts,
// the size of its data and the container's chunk count.
struct ProgramChunk {
    std::size_t offset        = 0;
    std::size_t bytes         = 0;
    std::uint32_t chunk_count = 0;
};

// The container is one the shader fixes understand: its header, a stored
// checksum equal to ours, a declared size equal to the blob's, chunk
// offsets inside it, and exactly one program chunk. Returns that chunk.
std::optional<ProgramChunk> find_program(std::span<const std::uint8_t> in) noexcept;

// The program chunk's tokens. `chunk` is one find_program returned for `in`.
std::vector<std::uint32_t> program_tokens(std::span<const std::uint8_t> in, const ProgramChunk& chunk);

// The cheap test each fix makes before the checksum: `in` starts with the
// DXBC magic and holds `pattern` somewhere. Most shaders fail it, and
// skipping them avoids an MD5 of each (about 20 us a shader on the thread
// that creates it).
bool may_hold(std::span<const std::uint8_t> in, std::span<const std::uint8_t> pattern) noexcept;

// The program of a container a fix has opened: its chunk, its tokens and
// where its instructions end (its declared length).
struct ShaderProgram {
    ProgramChunk chunk;
    std::vector<std::uint32_t> tokens;
    std::size_t end = 0;
};

// The program chunk of `in` (find_program) and its tokens, when the program
// is of `type` (kPixelShaderProgram, ...) and its declared length lies
// inside the chunk; otherwise nothing.
std::optional<ShaderProgram> open_program(std::span<const std::uint8_t> in, std::uint32_t type);

// The data of the one chunk `fourcc` of a container, or nothing when it has
// none, more than one, or one that does not fit.
std::optional<std::span<const std::uint8_t>> find_chunk(std::span<const std::uint8_t> in,
                                                        std::string_view fourcc) noexcept;

// The container with `prog` in place of the program chunk's tokens: the
// bytes up to the program, the program, the chunks after it, whose offsets
// move by the change in size; then the total size, the chunk size and the
// checksum. `prog` may be longer or shorter than the original program.
std::vector<std::uint8_t> reassemble(std::span<const std::uint8_t> in, const ProgramChunk& chunk,
                                     const std::vector<std::uint32_t>& prog);

} // namespace omvr::core::render
