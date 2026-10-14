#pragma once

#include <cstddef>
#include <filesystem>
#include <span>

// Builds the mod's UI archive (the game's UI with the options menu and VR
// controller glyphs) from the player's own DATA\UI.PAK and a patch embedded
// in the DLL, so no game files are distributed. The patch format is
// described in scripts/ui_patch.py, which makes it.
namespace mothervr::integration {

// Applies `patch` to the archive at `stock` and writes the result to
// `target` (PAK2). False, with nothing written, if the stock archive is not
// the one the patch was made for (another game version or language) or
// anything is malformed.
bool build_ui_archive(const std::filesystem::path& stock, std::span<const std::byte> patch,
                      const std::filesystem::path& target) noexcept;

} // namespace mothervr::integration
