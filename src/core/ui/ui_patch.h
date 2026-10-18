// OpenMotherVR-Provenance: clean
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

// A variant entry (spec 23 §6.2): an entry named `<name>` kVariantSeparator
// `<tag>` is an alternative form of the entry `<name>` before it, made
// from the same stock entry. It replaces that entry when `<tag>` is the
// variant asked for and is left out otherwise, so a patch without variant
// entries builds as before. '|' cannot be in a game file name.
inline constexpr char kVariantSeparator = '|';

// Applies `patch` to the archive at `stock` and writes the result to
// `target` (PAK2), with the variant entries tagged `variant` (none when
// empty). False, with nothing written, if the stock archive is not the one
// the patch was made for (another game version or language) or anything
// is malformed. Logs why. Game thread at attach.
bool build_ui_archive(ports::ArchiveFiles& files, diag::ports::Log& log, const std::filesystem::path& stock,
                      std::span<const std::byte> patch, const std::filesystem::path& target,
                      std::string_view variant = {}) noexcept;

// The variant tag of the terminal text size `scale` (spec 23 §6.2): empty
// for 1.0 (the movie as v1 links it), "text125", "text150" or "text175".
// `key` is 0 to 3, the step's place in config::kTerminalTextScales: part of
// the archive's staleness key.
struct UiVariant {
    std::string_view tag;
    int key = 0;
};
UiVariant terminal_text_variant(float scale) noexcept;

} // namespace omvr::core::ui
