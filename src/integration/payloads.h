#pragma once

#include <cstdint>
#include <filesystem>

#include <windows.h>

namespace mothervr::integration {

// Build the mod's UI archive at <game_dir>\DATA\MOTHER.PAK from the stock
// DATA\UI.PAK and the embedded patch (see ui_patch.h), unless a copy
// stamped with `build_stamp` (a FILETIME, see this_module_build_filetime())
// and built from the UI.PAK now there is already in place. The stamp is
// applied to all three file times so a mod update replaces the file exactly
// once; DATA\MOTHER.PAK.src records UI.PAK's size and last-write time, so a
// game update or a language switch rebuilds it too.
//
// Returns false if DATA\UI.PAK is missing (a message box is queued, see
// dialogs.h) or is not the version the patch was made for, or the write
// failed; the caller must then leave the game's UI archive alone (the
// options page is missing, VR still works).
//
// From: FUN_1001e720 @ 0x1001e720.
bool deploy_mother_pak(HMODULE self, const std::filesystem::path& game_dir,
                       std::int64_t build_stamp) noexcept;

// Same scheme for the embedded openvr_api.dll 1.0.10, written next to this
// DLL. The original does this in DllMain when the configured runtime is
// OpenVR; here the OpenVR backend calls it just before it loads the DLL, so
// the write is outside the loader lock, and it also happens when OpenVR is
// reached by fallback or chosen in the menu after startup.
//
// From: FUN_1001e940 @ 0x1001e940.
bool deploy_openvr_api() noexcept;

} // namespace mothervr::integration
