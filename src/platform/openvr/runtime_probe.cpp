// OpenMotherVR-Provenance: clean
#include "platform/openvr/runtime_probe.h"

#include "platform/openvr/constants.h"
#include "platform/win32/text.h"

#include <nlohmann/json.hpp>

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace omvr::platform::openvr {

namespace {

using core::diag::ports::logf;
using win32::utf8;

constexpr std::string_view kLogFound = "vr probe: OpenVR runtime at {}";
constexpr std::string_view kLogNoLoader =
    "vr probe: openvrpaths.vrpath points to {} but no openvr_api.dll there";
constexpr std::string_view kLogUnavailable = "vr probe: OpenVR not available: {}";

// Why no runtime path came out of openvrpaths.vrpath (11 §2.3: the log must
// tell a missing file from one that does not parse or has no runtime entry).
constexpr const char* kWhyNoFile    = "no openvrpaths.vrpath (SteamVR not installed or never run)";
constexpr const char* kWhyNotParsed = "openvrpaths.vrpath is not parseable";
constexpr const char* kWhyNoRuntime = "openvrpaths.vrpath has no runtime entry";
constexpr const char* kWhyNotRead   = "openvrpaths.vrpath could not be read";

// %LOCALAPPDATA%, or empty when it is not set.
std::wstring local_app_data() {
    DWORD n = ::GetEnvironmentVariableW(kLocalAppDataVariable, nullptr, 0);
    if (n == 0) return {};
    std::wstring value(n, L'\0');
    n = ::GetEnvironmentVariableW(kLocalAppDataVariable, value.data(), static_cast<DWORD>(value.size()));
    if (n == 0) return {};
    value.resize(n);
    return value;
}

// %LOCALAPPDATA%\openvr\openvrpaths.vrpath; the canonical OpenVR runtime
// registry.
std::filesystem::path openvr_paths_file() {
    const std::wstring base = local_app_data();
    if (base.empty()) return {};
    return std::filesystem::path{base} / kPathsDirectory / kPathsFileName;
}

// The first entry of the file's "runtime" array, or empty with `why` set.
std::filesystem::path runtime_entry(const std::filesystem::path& file, const char*& why) {
    std::ifstream f(file);
    nlohmann::json j;
    try {
        f >> j;
    } catch (...) {
        why = kWhyNotParsed;
        return {};
    }
    const auto it = j.find(kPathsRuntimeKey);
    if (it == j.end() || !it->is_array() || it->empty() || !it->at(0).is_string()) {
        why = kWhyNoRuntime;
        return {};
    }
    const auto text = it->at(0).get<std::string>(); // JSON text is UTF-8
    return std::filesystem::path{std::u8string(text.begin(), text.end())};
}

// The OpenVR runtime path from openvrpaths.vrpath, or empty on any
// failure, with `why` saying which.
std::filesystem::path read_openvr_runtime_path(const char*& why) noexcept {
    why = "";
    try {
        const auto file = openvr_paths_file();
        std::error_code ec;
        if (file.empty() || !std::filesystem::exists(file, ec)) {
            why = kWhyNoFile;
            return {};
        }
        return runtime_entry(file, why);
    } catch (...) {
        why = kWhyNotRead;
        return {};
    }
}

} // namespace

core::vr::ports::ProbeResult RuntimeProbe::probe() {
    core::vr::ports::ProbeResult result;
    const char* why    = "";
    const auto runtime = read_openvr_runtime_path(why);
    if (runtime.empty()) {
        logf(log_, kLogUnavailable, why);
        result.why = why;
        return result;
    }
    const auto loader = runtime / kBinDirectory / kX86Directory / kLoaderFileName;
    std::error_code ec;
    if (std::filesystem::exists(loader, ec)) {
        result.available = true;
        result.location  = runtime;
        logf(log_, kLogFound, utf8(runtime));
    } else {
        logf(log_, kLogNoLoader, utf8(runtime));
    }
    result.why = why;
    return result;
}

} // namespace omvr::platform::openvr
