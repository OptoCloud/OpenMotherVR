#pragma once

#include "core/diag/ports/log.h"
#include "core/ui/ports/archive_files.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

// Builds the mod's UI archive (the game's UI with the options menu and VR
// controller glyphs) from the player's own DATA\UI.PAK and a patch embedded
// in the DLL, so no game files are distributed. The patch format is
// described in scripts/ui_patch.py, which makes it.
namespace omvr::core::ui {

// The patch: "MVRUIP01", u32 alignment, u32 entry count, then per entry a
// u16 name length, the name and a u8 kind (PatchEntry), each kind followed
// by its own fields (scripts/ui_patch.py).
inline constexpr std::string_view kUiPatchMagic = "MVRUIP01";

// How an entry of the output archive is made.
enum class PatchEntry : std::uint8_t {
    Stock = 0, // the stock entry unchanged (u32 stock size, u32 stock CRC-32)
    Delta = 1, // copy and insert operations on the stock entry
    New   = 2, // bytes carried in the patch (u32 size, the bytes)
};

// One operation of a Delta entry.
enum class DeltaOp : std::uint8_t {
    Copy   = 0, // u32 stock offset, u32 length
    Insert = 1, // u32 length, the bytes
};

// Applies `patch` to the archive at `stock` and writes the result to
// `target` (PAK2). False, with nothing written, if the stock archive is not
// the one the patch was made for (another game version or language) or
// anything is malformed. Logs why. Game thread at attach.
bool build_ui_archive(ports::ArchiveFiles& files, diag::ports::Log& log, const std::filesystem::path& stock,
                      std::span<const std::byte> patch, const std::filesystem::path& target) noexcept;

} // namespace omvr::core::ui
