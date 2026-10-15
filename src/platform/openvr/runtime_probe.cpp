#include "platform/openvr/runtime_probe.h"

#include "core/diag/logger.h" // R: vr (A1): log through an injected Log&
#include "platform/win32/text.h"

#include <nlohmann/json.hpp>

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace omvr::platform::openvr {

namespace {

namespace diag {
using core::diag::logf;
using win32::utf8;
} // namespace diag

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

} // namespace

core::vr::ports::ProbeResult RuntimeProbe::probe() {
    core::vr::ports::ProbeResult result;
    // OpenVR via openvrpaths.vrpath.
    const char* why = "";
    const auto runtime = read_openvr_runtime_path(why);
    if (!runtime.empty()) {
        const auto api_dll = runtime / "bin" / "win32" / "openvr_api.dll";
        std::error_code ec;
        if (std::filesystem::exists(api_dll, ec)) {
            result.available = true;
            result.location  = runtime;
            diag::logf("vr probe: OpenVR runtime at {}", diag::utf8(runtime));
        } else {
            diag::logf("vr probe: openvrpaths.vrpath points to {} but no openvr_api.dll there", diag::utf8(runtime));
        }
    } else {
        diag::logf("vr probe: OpenVR not available: {}", why);
    }
    result.why = why;
    return result;
}

} // namespace omvr::platform::openvr
