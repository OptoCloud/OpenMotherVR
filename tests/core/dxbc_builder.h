// OpenMotherVR-Provenance: clean
#pragma once

// DXBC containers for the shader-fix tests, built from the public container
// layout (a header, the chunk offsets, then each chunk as its four-character
// code, its size and its data) independently of the patchers in
// core/render, which the tests check. Only the checksum is core's own, and
// it is checked against fxc's in render_test.cpp.

#include "core/render/dxbc.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

namespace omvr::tests {

// Byte offsets of the container header.
inline constexpr std::size_t kDxbcChecksumAt  = 4;
inline constexpr std::size_t kDxbcVersionAt   = 20;
inline constexpr std::size_t kDxbcTotalSizeAt = 24;
inline constexpr std::size_t kDxbcChunkCount  = 28;
inline constexpr std::size_t kDxbcChunkTable  = 32;
// A chunk's header: its four-character code, then its size.
inline constexpr std::size_t kChunkHeaderBytes = 8;

inline std::uint32_t u32_at(std::span<const std::uint8_t> b, std::size_t at) {
    std::uint32_t v = 0;
    std::memcpy(&v, b.data() + at, sizeof(v));
    return v;
}

// The bytes of 32-bit words, as the container stores them.
inline std::vector<std::uint8_t> bytes_of_words(const std::vector<std::uint32_t>& words) {
    std::vector<std::uint8_t> out(words.size() * sizeof(std::uint32_t));
    std::memcpy(out.data(), words.data(), out.size());
    return out;
}

// One chunk of a container: its four-character code and its data.
struct DxbcChunk {
    std::string_view fourcc;
    std::vector<std::uint8_t> data;
};

// `blob` with its checksum stored again.
inline void store_checksum(std::vector<std::uint8_t>& blob) {
    const core::render::DxbcChecksum sum = core::render::dxbc_checksum(blob);
    std::memcpy(blob.data() + kDxbcChecksumAt, sum.data(), sizeof(sum));
}

// A container of `chunks` in order, version 1, checksummed.
inline std::vector<std::uint8_t> container_of(const std::vector<DxbcChunk>& chunks) {
    std::size_t size = kDxbcChunkTable + 4 * chunks.size();
    for (const auto& c : chunks) size += kChunkHeaderBytes + c.data.size();
    std::vector<std::uint8_t> out(size);
    const auto put = [&out](std::size_t at, std::size_t v) {
        const auto u = static_cast<std::uint32_t>(v);
        std::memcpy(out.data() + at, &u, sizeof(u));
    };
    std::memcpy(out.data(), "DXBC", 4);
    put(kDxbcVersionAt, 1);
    put(kDxbcTotalSizeAt, out.size());
    put(kDxbcChunkCount, chunks.size());
    std::size_t at = kDxbcChunkTable + 4 * chunks.size();
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        put(kDxbcChunkTable + 4 * i, at);
        std::memcpy(out.data() + at, chunks[i].fourcc.data(), 4);
        put(at + 4, chunks[i].data.size());
        std::memcpy(out.data() + at + kChunkHeaderBytes, chunks[i].data.data(), chunks[i].data.size());
        at += kChunkHeaderBytes + chunks[i].data.size();
    }
    store_checksum(out);
    return out;
}

// A container with one SHEX chunk holding `program`, checksummed: the
// chunk at byte 36, the program at byte 44.
inline std::vector<std::uint8_t> container(const std::vector<std::uint32_t>& program) {
    return container_of({{"SHEX", bytes_of_words(program)}});
}

// The tokens of a container's SHEX chunk, found through its chunk table.
inline std::vector<std::uint32_t> program_of(std::span<const std::uint8_t> blob) {
    const std::uint32_t count = u32_at(blob, kDxbcChunkCount);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t at = u32_at(blob, kDxbcChunkTable + 4 * std::size_t(i));
        if (std::memcmp(blob.data() + at, "SHEX", 4) != 0) continue;
        std::vector<std::uint32_t> t(u32_at(blob, at + 4) / 4);
        std::memcpy(t.data(), blob.data() + at + kChunkHeaderBytes, t.size() * 4);
        return t;
    }
    FAIL_CHECK("no SHEX chunk");
    return {};
}

// `blob` with the word at `at` replaced and the checksum stored again.
inline std::vector<std::uint8_t> with_word(std::vector<std::uint8_t> blob, std::size_t at, std::uint32_t v) {
    std::memcpy(blob.data() + at, &v, sizeof(v));
    store_checksum(blob);
    return blob;
}

// The checksum stored in `blob` is the one core computes.
inline void check_stored_checksum(std::span<const std::uint8_t> blob) {
    const core::render::DxbcChecksum sum = core::render::dxbc_checksum(blob);
    for (std::size_t i = 0; i < sum.size(); ++i) CHECK(sum[i] == u32_at(blob, kDxbcChecksumAt + 4 * i));
}

} // namespace omvr::tests
