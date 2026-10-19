// OpenMotherVR-Provenance: clean
#include "core/render/dxbc.h"

#include "core/render/dxbc_tokens.h"
#include "util/hash/md5_block.h"

#include <algorithm>
#include <cstring>

namespace omvr::core::render {

namespace {

// RFC 1321 §3.3: MD5's initial state.
constexpr DxbcChecksum kMd5InitialState = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
constexpr std::size_t kMd5BlockBytes    = 64;
constexpr std::uint8_t kMd5Pad          = 0x80;
// DXBC's final block: the length in bits first, the rest of the data and
// the pad after it; a remainder that leaves no room for that (56 bytes or
// more) gets a block of its own and the length goes in a block of zeros.
// The last word is the length in bits over 4, with bit 0 set.
constexpr std::size_t kRoomForLength  = 56;
constexpr std::size_t kLastWordOffset = 60;
constexpr std::uint32_t kBitsPerByte  = 8;

constexpr std::size_t kImmediateValues4 = 4;
constexpr std::size_t kImmediateValues1 = 1;

} // namespace

std::optional<Operand> parse_operand(const std::uint32_t* t, std::size_t end, std::size_t i) {
    if (i >= end) return std::nullopt;
    const std::uint32_t tok = t[i];
    Operand o{i, 1, op_type(tok), 0};
    std::size_t p = i + 1;
    if (op_extended(tok)) ++p; // one modifier token (negate, abs)
    if (o.type == kTypeImmediate32) {
        p += (tok & kComponentCountMask) == kFourComponents ? kImmediateValues4 : kImmediateValues1;
    } else {
        const std::uint32_t dims = index_dim(tok);
        for (std::uint32_t d = 0; d < dims; ++d) {
            if (index_rep(tok, d) != kIndexImmediate32) return std::nullopt;
            if (p >= end) return std::nullopt; // the index lies past the instruction
            if (d == 0) o.reg = t[p];
            ++p;
        }
    }
    if (p > end) return std::nullopt;
    o.size = p - i;
    return o;
}

std::optional<std::vector<Instruction>> instructions(std::span<const std::uint32_t> t, std::size_t end) {
    std::vector<Instruction> out;
    for (std::size_t i = kProgramHeaderTokens; i < end;) {
        const std::uint32_t tok = t[i];
        const std::uint32_t op  = opcode(tok);
        std::size_t len         = inst_length(tok);
        if (op == kOpCustomData) {
            if (i + 1 >= end) return std::nullopt; // no room for its length
            len = t[i + 1];
        }
        // Not `i + len > end`: a customData length near 2^32 wraps size_t
        // on Win32 and would walk backwards forever.
        if (len == 0 || len > end - i) return std::nullopt;
        out.push_back({i, len, op, op_extended(tok)});
        i += len;
    }
    return out;
}

bool is_immediate4(const std::uint32_t* t, const Operand& o, std::uint32_t x, std::uint32_t y,
                   std::uint32_t z, std::uint32_t w) {
    if (o.type != kTypeImmediate32 || o.size != 1 + kImmediateValues4) return false;
    return t[o.at + 1] == x && t[o.at + 2] == y && t[o.at + 3] == z && t[o.at + 4] == w;
}

DxbcChecksum dxbc_checksum(std::span<const std::uint8_t> blob) noexcept {
    const std::uint8_t* data = blob.data() + kChecksummedFrom;
    const std::size_t size   = blob.size() - kChecksummedFrom;
    DxbcChecksum state       = kMd5InitialState;
    const std::size_t full   = size - size % kMd5BlockBytes;
    for (std::size_t off = 0; off < full; off += kMd5BlockBytes) util::md5_block(state.data(), data + off);
    const std::size_t left        = size - full;
    const std::uint32_t bits      = static_cast<std::uint32_t>(size * kBitsPerByte);
    const std::uint32_t last_word = (bits >> 2) | 1;
    std::uint8_t block[kMd5BlockBytes]{};
    if (left >= kRoomForLength) {
        std::memcpy(block, data + full, left);
        block[left] = kMd5Pad;
        util::md5_block(state.data(), block);
        std::memset(block, 0, sizeof(block));
        std::memcpy(block, &bits, sizeof(bits));
        std::memcpy(block + kLastWordOffset, &last_word, sizeof(last_word));
    } else {
        std::memcpy(block, &bits, sizeof(bits));
        std::memcpy(block + sizeof(bits), data + full, left);
        block[sizeof(bits) + left] = kMd5Pad;
        std::memcpy(block + kLastWordOffset, &last_word, sizeof(last_word));
    }
    util::md5_block(state.data(), block);
    return state;
}

std::uint32_t read_u32(std::span<const std::uint8_t> in, std::size_t at) noexcept {
    std::uint32_t v = 0;
    std::memcpy(&v, in.data() + at, sizeof(v));
    return v;
}

namespace {

// The bounds checks compare against what is left rather than summing: the
// counts, offsets and sizes are untrusted 32-bit values, and a sum wraps
// size_t on Win32. Each takes a container of at least the header's size.
bool chunk_offsets_fit(std::span<const std::uint8_t> in, std::uint32_t count) noexcept {
    return count <= (in.size() - kChunkOffsetsOffset) / kTokenBytes;
}

bool chunk_header_fits(std::span<const std::uint8_t> in, std::uint32_t off) noexcept {
    return off <= in.size() - kChunkHeaderSize;
}

// After chunk_header_fits(in, off).
bool chunk_data_fits(std::span<const std::uint8_t> in, std::uint32_t off, std::uint32_t bytes) noexcept {
    return bytes <= in.size() - off - kChunkHeaderSize;
}

bool is_program_chunk(const std::uint8_t* chunk) noexcept {
    return std::memcmp(chunk, kProgramChunkSm5.data(), kFourCcBytes) == 0 ||
           std::memcmp(chunk, kProgramChunkSm4.data(), kFourCcBytes) == 0;
}

} // namespace

std::optional<ProgramChunk> find_program(std::span<const std::uint8_t> in) noexcept {
    if (in.size() < kContainerHeaderSize) return std::nullopt;
    const DxbcChecksum sum = dxbc_checksum(in);
    if (std::memcmp(sum.data(), in.data() + kChecksumOffset, kChecksumBytes) != 0) return std::nullopt;
    const std::uint32_t declared = read_u32(in, kTotalSizeOffset);
    ProgramChunk p;
    p.chunk_count = read_u32(in, kChunkCountOffset);
    if (declared != in.size() || !chunk_offsets_fit(in, p.chunk_count)) return std::nullopt;
    for (std::uint32_t c = 0; c < p.chunk_count; ++c) {
        const std::uint32_t off = read_u32(in, kChunkOffsetsOffset + kTokenBytes * std::size_t(c));
        if (!chunk_header_fits(in, off)) return std::nullopt;
        if (!is_program_chunk(in.data() + off)) continue;
        if (p.offset) return std::nullopt;
        const std::uint32_t bytes = read_u32(in, off + kChunkSizeOffset);
        if (!chunk_data_fits(in, off, bytes) || bytes % kTokenBytes) return std::nullopt;
        p.offset = off;
        p.bytes  = bytes;
    }
    if (!p.offset || p.bytes < kProgramHeaderTokens * kTokenBytes) return std::nullopt;
    return p;
}

std::vector<std::uint32_t> program_tokens(std::span<const std::uint8_t> in, const ProgramChunk& chunk) {
    std::vector<std::uint32_t> t(chunk.bytes / kTokenBytes);
    std::memcpy(t.data(), in.data() + chunk.offset + kChunkHeaderSize, chunk.bytes);
    return t;
}

bool may_hold(std::span<const std::uint8_t> in, std::span<const std::uint8_t> pattern) noexcept {
    if (in.size() < kContainerHeaderSize || std::memcmp(in.data(), kDxbcMagic.data(), kDxbcMagicBytes) != 0)
        return false;
    return !std::ranges::search(in, pattern).empty();
}

std::optional<ShaderProgram> open_program(std::span<const std::uint8_t> in, std::uint32_t type) {
    const auto chunk = find_program(in);
    if (!chunk) return std::nullopt;
    ShaderProgram p{*chunk, program_tokens(in, *chunk), 0};
    // find_program leaves at least the header's two tokens.
    const std::vector<std::uint32_t>& t = p.tokens;
    if (program_type(t[0]) != type) return std::nullopt;
    if (t[1] < kProgramHeaderTokens || t[1] > t.size()) return std::nullopt;
    p.end = t[1];
    return p;
}

std::optional<std::span<const std::uint8_t>> find_chunk(std::span<const std::uint8_t> in,
                                                        std::string_view fourcc) noexcept {
    if (in.size() < kContainerHeaderSize || fourcc.size() != kFourCcBytes) return std::nullopt;
    const std::uint32_t count = read_u32(in, kChunkCountOffset);
    if (!chunk_offsets_fit(in, count)) return std::nullopt;
    std::optional<std::span<const std::uint8_t>> found;
    for (std::uint32_t c = 0; c < count; ++c) {
        const std::uint32_t off = read_u32(in, kChunkOffsetsOffset + kTokenBytes * std::size_t(c));
        if (!chunk_header_fits(in, off)) return std::nullopt;
        if (std::memcmp(in.data() + off, fourcc.data(), kFourCcBytes) != 0) continue;
        const std::uint32_t bytes = read_u32(in, off + kChunkSizeOffset);
        if (found || !chunk_data_fits(in, off, bytes)) return std::nullopt;
        found = in.subspan(std::size_t(off) + kChunkHeaderSize, bytes);
    }
    return found;
}

std::vector<std::uint8_t> reassemble(std::span<const std::uint8_t> in, const ProgramChunk& chunk,
                                     const std::vector<std::uint32_t>& prog) {
    const std::size_t prog_bytes = prog.size() * kTokenBytes;
    // The program may grow (the dither and sprite fixes) or shrink (the
    // constant smoke lighting): every size and later offset moves by `delta`.
    const auto delta = static_cast<std::ptrdiff_t>(prog_bytes) - static_cast<std::ptrdiff_t>(chunk.bytes);
    const std::size_t data_at = chunk.offset + kChunkHeaderSize;
    std::vector<std::uint8_t> out;
    out.reserve(in.size() + prog_bytes);
    out.insert(out.end(), in.begin(), in.begin() + static_cast<std::ptrdiff_t>(data_at));
    out.resize(data_at + prog_bytes);
    std::memcpy(out.data() + data_at, prog.data(), prog_bytes);
    out.insert(out.end(), in.begin() + static_cast<std::ptrdiff_t>(data_at + chunk.bytes), in.end());

    const auto put = [&out](std::size_t at, std::size_t v) {
        const auto u = static_cast<std::uint32_t>(v);
        std::memcpy(out.data() + at, &u, sizeof(u));
    };
    put(kTotalSizeOffset, out.size());
    put(chunk.offset + kChunkSizeOffset, prog_bytes);
    for (std::uint32_t c = 0; c < chunk.chunk_count; ++c) {
        const std::size_t entry = kChunkOffsetsOffset + kTokenBytes * std::size_t(c);
        const std::uint32_t off = read_u32(in, entry);
        if (off > chunk.offset)
            put(entry, static_cast<std::size_t>(static_cast<std::ptrdiff_t>(off) + delta));
    }
    const DxbcChecksum sum = dxbc_checksum(out);
    std::memcpy(out.data() + kChecksumOffset, sum.data(), kChecksumBytes);
    return out;
}

} // namespace omvr::core::render
