#include "core/ui/compat_messages.h"

#include "core/vr/runtime_failure.h"

#include <format>

namespace omvr::core::ui {

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

void report_init_failure(ports::Dialogs& dialogs, vr::detect_result::Chosen runtime) noexcept {
    using vr::detect_result;
    constexpr std::string_view kDisabled =
        "MotherVR will now start with VR disabled. Restart the game to try again.";
    const auto& f = vr::last_init_failure();
    std::string text;
    if (runtime == detect_result::Chosen::OpenVR && f.code == 117) { // VRInitError_Init_AnotherAppLaunching
        text = std::format(
            "Failed to initialize SteamVR!\n[{} | {}]\n\n"
            "Look into the above error, and try again once it has been resolved.\n\n"
            "It's possible that this error is being caused by SteamVR Desktop Game Theatre.\n\n"
            "To use MotherVR with SteamVR, you need to uncheck 'Use Desktop Game Theatre while "
            "SteamVR is active' in the Steam Properties for Alien: Isolation, and make sure that "
            "VR Game Theatre isn't running when you start the game.\n\n{}",
            f.what, f.code, kDisabled);
    } else if (runtime == detect_result::Chosen::OpenVR) {
        text = std::format(
            "Failed to initialize SteamVR!\n[{} | {}]\n\n"
            "Look into the above error, and try again once it has been resolved.\n\n{}",
            f.what.empty() ? "unknown error" : f.what, f.code, kDisabled);
    } else {
        // The choice is kept, so a restart retries OpenXR: say how to get back.
        constexpr int kRuntimeUnavailable = -51; // XR_ERROR_RUNTIME_UNAVAILABLE
        const std::string_view cause =
            f.code == kRuntimeUnavailable
                ? "No usable 32-bit OpenXR runtime could be loaded. mothervr.log lists the "
                  "runtimes that were tried.\n\n"
                : "";
        text = std::format(
            "Failed to initialize OpenXR!\n[{} | {}]\n\n{}"
            "Look into the above error, and try again once it has been resolved.\n\n"
            "To go back to SteamVR, choose \"Use SteamVR\" on the MotherVR options page, "
            "or start the game with -vrforceopenvr.\n\n{}",
            f.what.empty() ? "unknown error" : f.what, f.code, cause, kDisabled);
    }
    dialogs.show_now(text, ports::DialogKind::Error, "runtime failed");
}

} // namespace omvr::core::ui
