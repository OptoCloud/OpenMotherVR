#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

// The game's PAK2 archive format, in memory.
namespace omvr::core::ui {

// PAK2: "PAK2", u32 names size, u32 count, u32 alignment, NUL-terminated
// names, u32 end offset per entry, data; each entry starts at the previous
// end (or the end of the table) rounded up to the alignment. The entries
// view into `pak`; nullopt when it is malformed.
std::optional<std::unordered_map<std::string, std::span<const std::byte>>> read_pak(
    std::span<const std::byte> pak);

struct OutEntry {
    std::string name;
    std::vector<std::byte> data;
};

// The archive holding `entries` in order, with `align`.
std::vector<std::byte> build_pak(std::uint32_t align, const std::vector<OutEntry>& entries);

} // namespace omvr::core::ui
