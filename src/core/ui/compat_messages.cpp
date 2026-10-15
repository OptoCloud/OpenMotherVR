#include "core/ui/compat_messages.h"

#include "core/ui/tuning.h"

#include <format>

namespace omvr::core::ui {

namespace {

constexpr std::string_view kDisabled =
    "MotherVR will now start with VR disabled. Restart the game to try again.";
constexpr std::string_view kUnknownError = "unknown error";

std::string_view or_unknown(std::string_view what) noexcept { return what.empty() ? kUnknownError : what; }

// SteamVR's Desktop Game Theatre held the headset.
std::string openvr_theatre_text(std::string_view what, int code) {
    return std::format(
        "Failed to initialize SteamVR!\n[{} | {}]\n\n"
        "Look into the above error, and try again once it has been resolved.\n\n"
        "It's possible that this error is being caused by SteamVR Desktop Game Theatre.\n\n"
        "To use MotherVR with SteamVR, you need to uncheck 'Use Desktop Game Theatre while "
        "SteamVR is active' in the Steam Properties for Alien: Isolation, and make sure that "
        "VR Game Theatre isn't running when you start the game.\n\n{}",
        what, code, kDisabled);
}

std::string openvr_text(std::string_view what, int code) {
    return std::format(
        "Failed to initialize SteamVR!\n[{} | {}]\n\n"
        "Look into the above error, and try again once it has been resolved.\n\n{}",
        or_unknown(what), code, kDisabled);
}

// The choice is kept, so a restart retries OpenXR: say how to get back.
std::string openxr_text(std::string_view what, int code) {
    const std::string_view cause =
        code == kOpenXrRuntimeUnavailable
            ? "No usable 32-bit OpenXR runtime could be loaded. mothervr.log lists the "
              "runtimes that were tried.\n\n"
            : "";
    return std::format(
        "Failed to initialize OpenXR!\n[{} | {}]\n\n{}"
        "Look into the above error, and try again once it has been resolved.\n\n"
        "To go back to SteamVR, choose \"Use SteamVR\" on the MotherVR options page, "
        "or start the game with -vrforceopenvr.\n\n{}",
        or_unknown(what), code, cause, kDisabled);
}

} // namespace

std::string exe_check_failed(std::uint32_t stamp) {
    return std::format(
        "Exe check failed!\n[0x{:X}]\n\n"
        "This check is to ensure that MotherVR is correctly modifying the game, and is looking in "
        "the correct locations.\n\n"
        "If Alien: Isolation is on an out-dated version, make sure to update through Steam or Epic. "
        "If a new update to the game has just been released, or this check failed for some other "
        "reason, MotherVR may need to be updated.\n\n"
        "MotherVR will now be disabled, and Alien: Isolation will startup normally.",
        stamp);
}

std::string_view welcome() noexcept {
    return "Welcome to MotherVR for Alien: Isolation!\n\n"
           "VR starts with SteamVR. To use OpenXR or turn VR off, go to Options->MotherVR after the "
           "game starts up. This message is only shown on the first launch of MotherVR.";
}

std::string ui_pak_missing(std::string_view path) {
    return "Failed to find UI.PAK!\n[" + std::string(path) +
           "]\n\nThe MotherVR options page will be unavailable for this session.";
}

std::string audio_failed(unsigned hr) { return std::format("Failed to hook Audio!\n[0x{:X}]", hr); }

std::string init_failure_text(vr::DetectResult::Chosen runtime, std::string_view what, int code) {
    if (runtime == vr::DetectResult::Chosen::OpenVR && code == kOpenVrAnotherAppLaunching)
        return openvr_theatre_text(what, code);
    if (runtime == vr::DetectResult::Chosen::OpenVR) return openvr_text(what, code);
    return openxr_text(what, code);
}

void report_init_failure(ports::Dialogs& dialogs, vr::DetectResult::Chosen runtime,
                         const vr::InitFailure& failure) noexcept {
    dialogs.show_now(init_failure_text(runtime, failure.what, failure.code), ports::DialogKind::Error,
                     "runtime failed");
}

} // namespace omvr::core::ui
