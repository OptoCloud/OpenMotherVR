#pragma once

#include "core/ui/ports/dialogs.h"
#include "core/vr/runtime_choice.h"

#include <cstdint>
#include <string>
#include <string_view>

// The messages the player sees, with the original's wording.
namespace omvr::core::ui {

// AI.exe is a build the mod does not know; `stamp` is its PE timestamp.
std::string exe_check_failed(std::uint32_t stamp);

// The first launch. It defaulted to VR off in the original; this build
// starts with SteamVR, so the text says where to change that instead.
// From: FUN_1000ce80 (first-run branch).
std::string_view welcome() noexcept;

// DATA\UI.PAK is missing at `path` (UTF-8).
std::string ui_pak_missing(std::string_view path);

// The headset's audio could not be hooked.
std::string audio_failed(unsigned hr);

// Tell the player that VR could not start, with the original's wording.
// From: the dialogs in FUN_100068b0 (OpenVR) and FUN_100037b0 (Oculus).
void report_init_failure(ports::Dialogs& dialogs, vr::detect_result::Chosen runtime) noexcept;

} // namespace omvr::core::ui
