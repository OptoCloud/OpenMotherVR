// Probe for installed VR runtimes and choose which backend MotherVR should
// drive. Discovery reads each runtime's published install location without
// loading any of its DLLs.
//
//   OpenVR: parse %LOCALAPPDATA%\openvr\openvrpaths.vrpath (JSON written by
//           SteamVR's vrpathreg.exe), pick the first entry from "runtime"[],
//           check <runtime>\bin\win32\openvr_api.dll exists.
//   OpenXR: the active runtime from the Khronos registry key, or its 32-bit
//           sibling manifest (openxr::probe).
//   Oculus: not probed. LibOVR is not driven directly; Oculus/Meta headsets
//           run through OpenXR (-vrforceoculus selects it).
//
// Choice precedence is documented in vr_detect.h. No runtime is started
// here: the chosen backend starts in the game's Stereo::CreateDevice.

#include "vr_detect.h"
#include "diag/log.h"
#include "openxr_backend.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <windows.h>
#include <shellapi.h>

namespace mothervr::runtime {

namespace {

// ---- cached detect result ---------------------------------------------------
detect_result g_last{};

// %LOCALAPPDATA%\openvr\openvrpaths.vrpath; canonical OpenVR runtime registry.
std::filesystem::path openvr_paths_file() {
    DWORD n = ::GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (n == 0) return {};
    std::wstring local_app_data(n, L'\0');
    n = ::GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data.data(),
                                  static_cast<DWORD>(local_app_data.size()));
    if (n == 0) return {};
    local_app_data.resize(n);
    return std::filesystem::path{local_app_data} / "openvr" / "openvrpaths.vrpath";
}

// Returns the OpenVR runtime path from openvrpaths.vrpath, or empty on any
// failure, with `why` saying which (11 §2.3: the log must tell a missing
// file from one that does not parse or has no runtime entry).
std::filesystem::path read_openvr_runtime_path(const char*& why) noexcept {
    why = "";
    try {
        auto path = openvr_paths_file();
        std::error_code ec;
        if (path.empty() || !std::filesystem::exists(path, ec)) {
            why = "no openvrpaths.vrpath (SteamVR not installed or never run)";
            return {};
        }
        std::ifstream f(path);
        nlohmann::json j;
        try {
            f >> j;
        } catch (...) {
            why = "openvrpaths.vrpath is not parseable";
            return {};
        }
        auto it = j.find("runtime");
        if (it == j.end() || !it->is_array() || it->empty() || !it->at(0).is_string()) {
            why = "openvrpaths.vrpath has no runtime entry";
            return {};
        }
        const auto utf8 = it->at(0).get<std::string>();   // JSON text is UTF-8
        return std::filesystem::path{std::u8string(utf8.begin(), utf8.end())};
    } catch (...) {
        why = "openvrpaths.vrpath could not be read";
        return {};
    }
}

// -vrforcenone / -vrforceopenvr / -vrforceoculus / -vrforceopenxr: an exact,
// case-insensitive argument, as the original matches them. The Oculus backend
// is served by OpenXR.
// From: FUN_1001f700.
std::optional<config::VRRuntime> forced_runtime() noexcept {
    struct Switch { const wchar_t* name; config::VRRuntime runtime; };
    static constexpr Switch kSwitches[] = {
        {L"-vrforcenone",   config::VRRuntime::None},
        {L"-vrforceopenvr", config::VRRuntime::OpenVR},
        {L"-vrforceoculus", config::VRRuntime::OpenXR},
        {L"-vrforceopenxr", config::VRRuntime::OpenXR},
    };
    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argv) return {};
    std::optional<config::VRRuntime> forced;
    for (int i = 1; i < argc; ++i)
        for (const auto& s : kSwitches)
            if (::_wcsicmp(argv[i], s.name) == 0) forced = s.runtime;
    ::LocalFree(argv);
    return forced;
}

init_failure g_failure;

} // namespace

const char* to_string(detect_result::Chosen c) noexcept {
    switch (c) {
        case detect_result::Chosen::OpenVR: return "OpenVR";
        case detect_result::Chosen::Oculus: return "Oculus";
        case detect_result::Chosen::OpenXR: return "OpenXR";
        case detect_result::Chosen::None:   return "None";
    }
    return "None";
}

detect_result detect() noexcept {
    detect_result r{};

    // OpenVR via openvrpaths.vrpath.
    const char* why = "";
    const auto runtime = read_openvr_runtime_path(why);
    if (!runtime.empty()) {
        const auto api_dll = runtime / "bin" / "win32" / "openvr_api.dll";
        std::error_code ec;
        if (std::filesystem::exists(api_dll, ec)) {
            r.openvr_available    = true;
            r.openvr_runtime_path = runtime;
            diag::logf("vr probe: OpenVR runtime at {}", diag::utf8(runtime));
        } else {
            diag::logf("vr probe: openvrpaths.vrpath points to {} but no openvr_api.dll there", diag::utf8(runtime));
        }
    } else {
        diag::logf("vr probe: OpenVR not available: {}", why);
    }

    // Oculus: served by OpenXR, see the top of the file.
    r.oculus_available = false;

    r.openxr_manifest  = openxr::probe();
    r.openxr_available = !r.openxr_manifest.empty();

    const auto forced = forced_runtime();
    r.forced    = forced.has_value();
    r.preferred = forced.value_or(config::current().vr_runtime);

    diag::logf("vr probe: openvr={} oculus={} openxr={} preferred={}{}",
               r.openvr_available, r.oculus_available, r.openxr_available,
               config::to_string(r.preferred), r.forced ? " (forced)" : "");

    if (r.preferred == config::VRRuntime::None) {
        r.chosen = detect_result::Chosen::None; // user picked "Disable VR"
    } else if (r.preferred == config::VRRuntime::OpenVR && r.openvr_available) {
        r.chosen = detect_result::Chosen::OpenVR;
    } else if (r.preferred == config::VRRuntime::Oculus && r.oculus_available) {
        r.chosen = detect_result::Chosen::Oculus;
    } else if (r.preferred == config::VRRuntime::OpenXR && r.openxr_available) {
        r.chosen = detect_result::Chosen::OpenXR;
    } else if (r.forced) {
        r.chosen = detect_result::Chosen::None; // forced runtime missing
    } else if (r.openvr_available) {
        r.chosen = detect_result::Chosen::OpenVR;
    } else if (r.oculus_available) {
        r.chosen = detect_result::Chosen::Oculus;
    } else if (r.openxr_available) {
        r.chosen = detect_result::Chosen::OpenXR;
    } else {
        r.chosen = detect_result::Chosen::None;
    }

    diag::logf("vr probe: chosen={}", to_string(r.chosen));
    g_last = r;
    return r;
}

const detect_result& last_result() noexcept {
    static const bool detected = (detect(), true); // once, thread-safe
    (void) detected;
    return g_last;
}

void set_init_failure(std::string what, int code) noexcept {
    g_failure = {std::move(what), code};
}

const init_failure& last_init_failure() noexcept {
    return g_failure;
}

void report_init_failure(detect_result::Chosen runtime) noexcept {
    constexpr std::string_view kDisabled =
        "MotherVR will now start with VR disabled. Restart the game to try again.";
    const auto& f = g_failure;
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
    diag::message_box(text, MB_OK | MB_ICONERROR);
}

} // namespace mothervr::runtime
