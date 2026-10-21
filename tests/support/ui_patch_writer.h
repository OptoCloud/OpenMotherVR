// OpenMotherVR-Provenance: clean
#pragma once

// A UI patch (MVRUIP01) built byte by byte, as scripts/ui_patch.py writes
// it, shared by the UI and HUD tests so a change to the format is made in
// one place.

#include "core/ui/ui_patch.h"
#include "fakes/fakes.h"
#include "util/hash/crc32.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace omvr::tests {

class PatchWriter {
public:
    PatchWriter(std::uint32_t align, std::uint32_t count) {
        text("MVRUIP01");
        u32(align);
        u32(count);
    }
    void entry(std::string_view name, core::ui::PatchEntry kind) {
        u16(static_cast<std::uint16_t>(name.size()));
        text(name);
        u8(static_cast<std::uint8_t>(kind));
    }
    // A stock entry: kept as `data`, checked by its size and CRC-32.
    void stock(std::string_view name, std::string_view data) {
        entry(name, core::ui::PatchEntry::Stock);
        u32(static_cast<std::uint32_t>(data.size()));
        u32(util::crc32(bytes_of(data)));
    }
    // A new entry holding `data`.
    void added(std::string_view name, std::string_view data) {
        entry(name, core::ui::PatchEntry::New);
        u32(static_cast<std::uint32_t>(data.size()));
        text(data);
    }
    void u8(std::uint8_t v) { out.push_back(std::byte{v}); }
    void u16(std::uint16_t v) { raw(&v, sizeof(v)); }
    void u32(std::uint32_t v) { raw(&v, sizeof(v)); }
    void text(std::string_view s) { raw(s.data(), s.size()); }

    std::vector<std::byte> out;

private:
    void raw(const void* p, std::size_t n) {
        const auto* b = static_cast<const std::byte*>(p);
        out.insert(out.end(), b, b + n);
    }
};

} // namespace omvr::tests
