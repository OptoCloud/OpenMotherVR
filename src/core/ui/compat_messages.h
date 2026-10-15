#pragma once

#include "core/ui/ports/dialogs.h"
#include "core/vr/runtime_choice.h"
#include "core/vr/runtime_failure.h"

#include <cstdint>
#include <string>
#include <string_view>

// The messages the player sees, with the original's wording. Pure text
// builders, except report_init_failure, which shows its text.
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

// The text telling the player that `runtime` could not start, with the
// runtime's error text `what` and code `code`.
// From: the dialogs in FUN_100068b0 (OpenVR) and FUN_100037b0 (Oculus).
std::string init_failure_text(vr::DetectResult::Chosen runtime, std::string_view what, int code);

// Shows init_failure_text for `failure` and waits for the player to close
// it. The thread that creates the stereo device.
void report_init_failure(ports::Dialogs& dialogs, vr::DetectResult::Chosen runtime,
                         const vr::InitFailure& failure) noexcept;

} // namespace omvr::core::ui
