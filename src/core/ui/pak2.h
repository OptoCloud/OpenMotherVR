// OpenMotherVR-Provenance: clean
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The game's PAK2 archive format, in memory (spec 14 §5).
namespace omvr::core::ui {

// PAK2: "PAK2", u32 names size, u32 count, u32 alignment, NUL-terminated
// names, u32 end offset per entry, data; each entry starts at the previous
// end (or the end of the table) rounded up to the alignment.
inline constexpr std::string_view kPak2Magic = "PAK2";
// The magic and the three u32 fields after it.
inline constexpr std::size_t kPak2HeaderSize = kPak2Magic.size() + 3 * sizeof(std::uint32_t);
// One u32 end offset per entry.
inline constexpr std::size_t kPak2OffsetSize = sizeof(std::uint32_t);

// The entries of `pak` by name, viewing into it; nullopt when it is
// malformed. Pure.
std::optional<std::unordered_map<std::string, std::span<const std::byte>>> read_pak(
    std::span<const std::byte> pak);

struct OutEntry {
    std::string name;
    std::vector<std::byte> data;
};

// The archive holding `entries` in order, with `align`. Pure.
std::vector<std::byte> build_pak(std::uint32_t align, const std::vector<OutEntry>& entries);

} // namespace omvr::core::ui
