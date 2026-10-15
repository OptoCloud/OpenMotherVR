// OpenMotherVR-Provenance: clean
// Finding an OpenXR runtime from a 32-bit process, and pointing the loader
// at it (09 §1).

#include "platform/openxr/runtime_probe.h"

#include "platform/openxr/constants.h"
#include "platform/openxr/session_impl.h"
#include "platform/win32/patching/pe_image.h"
#include "platform/win32/environment.h"
#include "platform/win32/process_info.h"
#include "platform/win32/registry.h"
#include "platform/win32/text.h"

#include <nlohmann/json.hpp>

#include <cwchar>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace omvr::platform::openxr {

namespace {

using core::diag::ports::logf;
using win32::utf8;

constexpr std::string_view kLogManifestLibrary    = "openxr: manifest {} -> {} (machine 0x{:04X})";
constexpr std::string_view kLogManifestUnreadable = "openxr: cannot read manifest {}: {}";
constexpr std::string_view kLogNoLoaderInit =
    "openxr: loader has no xrInitializeLoaderKHR; only XR_RUNTIME_JSON points it at the manifest";
constexpr std::string_view kLogPropertyRejected =
    "openxr: loader property not set: {}; only XR_RUNTIME_JSON points the loader at {}";
constexpr std::string_view kLogPropertyAccepted =
    "openxr: loader directed to {} (initialisation property accepted)";
constexpr std::string_view kLogElevated =
    "openxr: the game runs elevated (as administrator); the loader ignores the environment, "
    "only its initialisation property points it at the runtime";
constexpr std::string_view kLogPlayerChoice = "openxr: available: {} (XR_RUNTIME_JSON set by the player)";
constexpr std::string_view kLogActive       = "openxr: active runtime 64-bit {}, 32-bit {}";
constexpr std::string_view kLogNoneActive   = "openxr: not available: no active runtime registered";
constexpr std::string_view kLogNo32Bit      = "openxr: not available: no active runtime has a 32-bit build";
constexpr std::string_view kLogAvailable    = "openxr: available: {} ({})";
constexpr std::string_view kLogProbeFailed  = "openxr: probe failed: {}";

// The manifest's runtime library, relative to the manifest or absolute.
constexpr const char* kManifestRuntime     = "runtime";
constexpr const char* kManifestLibraryPath = "library_path";

// The active runtime's manifest in one registry view, or empty. A path of
// whatever length the registry holds, expanded when it is REG_EXPAND_SZ
// (09 §1): the loader expands it, so must we, or the manifest is not found.
std::filesystem::path active_runtime(win32::RegistryView view) {
    return win32::read_machine_string(kActiveRuntimeKey, kActiveRuntimeValue, view).value_or(std::wstring{});
}

// A manifest is usable from this process if its runtime library is x86.
bool usable_from_x86(core::diag::ports::Log& log, const std::filesystem::path& manifest) {
    std::error_code ec;
    if (manifest.empty() || !std::filesystem::exists(manifest, ec)) return false;
    try {
        std::ifstream f(manifest);
        const auto j        = nlohmann::json::parse(f);
        const auto lib_utf8 = j.at(kManifestRuntime).at(kManifestLibraryPath).get<std::string>();
        std::filesystem::path lib{std::u8string(lib_utf8.begin(), lib_utf8.end())};
        if (lib.is_relative()) lib = manifest.parent_path() / lib;
        const WORD m = win32::pe_machine(lib);
        logf(log, kLogManifestLibrary, utf8(manifest), utf8(lib), m);
        return m == IMAGE_FILE_MACHINE_I386;
    } catch (const std::exception& e) {
        logf(log, kLogManifestUnreadable, utf8(manifest), e.what());
        return false;
    }
}

// 32-bit manifests usually sit next to the 64-bit one with "32" in place of
// "64" in the name (steamxr_win64.json -> steamxr_win32.json,
// oculus_openxr_64.json -> oculus_openxr_32.json).
std::vector<std::filesystem::path> x86_siblings(const std::filesystem::path& manifest64) {
    std::vector<std::filesystem::path> out;
    const std::wstring name = manifest64.filename().wstring();
    for (const auto& [from, to] :
         {std::pair{L"win64", L"win32"}, std::pair{L"x64", L"x86"}, std::pair{L"64", L"32"}}) {
        if (const auto pos = name.rfind(from); pos != std::wstring::npos) {
            std::wstring n = name;
            n.replace(pos, std::wcslen(from), to);
            out.push_back(manifest64.parent_path() / n);
        }
    }
    return out;
}

// XR_RUNTIME_JSON as the player set it, if set and not empty.
std::optional<std::filesystem::path> runtime_json_override() {
    const auto env = win32::crt_environment_variable(kRuntimeJsonVariable);
    if (!env || env->empty()) return std::nullopt;
    return std::filesystem::path{*env};
}

// A manifest this process can load, and which rule found it.
struct Choice {
    std::filesystem::path manifest;
    const char* rule = "";
};

// SteamVR and most runtimes' "make active" write only the 64-bit view, so a
// different 32-bit entry is usually left over from a runtime used before:
// prefer the 32-bit build of the one the user made active, and take the
// 32-bit entry only when that has none (09 §1).
Choice choose_manifest(core::diag::ports::Log& log, const std::filesystem::path& m64,
                       const std::filesystem::path& m32) {
    if (usable_from_x86(log, m64)) return {m64, "the 64-bit registration"};
    for (const auto& m : x86_siblings(m64)) {
        if (usable_from_x86(log, m)) return {m, "the 32-bit sibling of the 64-bit registration"};
    }
    if (usable_from_x86(log, m32)) return {m32, "the 32-bit registration"};
    return {};
}

} // namespace

void Session::Impl::point_loader_at(const std::filesystem::path& manifest_path) noexcept {
    if (manifest_path.empty()) return;
    PFN_xrInitializeLoaderKHR init_loader = nullptr;
    if (XR_FAILED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                        reinterpret_cast<PFN_xrVoidFunction*>(&init_loader))) ||
        !init_loader) {
        log.line(kLogNoLoaderInit);
        return;
    }
    const std::string path = utf8(manifest_path);
    XrLoaderInitPropertyValueEXT value{kRuntimeJsonName, path.c_str()};
    XrLoaderInitInfoPropertiesEXT props{XR_TYPE_LOADER_INIT_INFO_PROPERTIES_EXT};
    props.propertyValueCount = 1;
    props.propertyValues     = &value;
    const XrResult r         = init_loader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&props));
    // Either outcome is logged (09 §1): this is the first thing to check
    // when "OpenXR does not start" is reported from an elevated game.
    if (XR_FAILED(r))
        logf(log, kLogPropertyRejected, static_cast<int>(r), path);
    else
        logf(log, kLogPropertyAccepted, path);
}

// Hands the chosen manifest to the loader through the environment (this
// module's CRT copy and the process), only once OpenXR is the runtime that
// starts: a probe for a runtime the player did not choose must not leave the
// variable to the game's child processes (01 §5.2). The elevated line goes
// with it, since only then is the loader property the one that counts.
void Session::Impl::set_runtime_environment(const std::filesystem::path& manifest_path) noexcept {
    if (manifest_path.empty()) return;
    win32::set_crt_environment_variable(kRuntimeJsonVariable, manifest_path.native());
    if (!elevation_logged && win32::process_elevated()) {
        elevation_logged = true;
        log.line(kLogElevated);
    }
}

std::filesystem::path Session::Impl::probe() noexcept {
    try {
        // An explicit override wins, as with the loader itself.
        if (auto set = runtime_json_override()) {
            // The player chose: left as it is, nothing else passed (09 §1).
            manifest.clear();
            logf(log, kLogPlayerChoice, utf8(*set));
            return *set;
        }
        const auto m64 = active_runtime(win32::RegistryView::Bits64);
        const auto m32 = active_runtime(win32::RegistryView::Bits32);
        logf(log, kLogActive, m64.empty() ? "(none)" : utf8(m64), m32.empty() ? "(none)" : utf8(m32));
        const Choice chosen = choose_manifest(log, m64, m32);
        if (chosen.manifest.empty()) {
            log.line(m64.empty() && m32.empty() ? kLogNoneActive : kLogNo32Bit);
            return {};
        }
        logf(log, kLogAvailable, utf8(chosen.manifest), chosen.rule);
        // Handed to the loader (environment and loader property) only when
        // OpenXR is the runtime that starts: init_system. Probing alone
        // changes nothing in the process (01 §5.2).
        manifest = chosen.manifest;
        return chosen.manifest;
    } catch (const std::exception& e) {
        logf(log, kLogProbeFailed, e.what());
    }
    return {};
}

core::vr::ports::ProbeResult RuntimeProbe::probe() {
    core::vr::ports::ProbeResult result;
    result.location  = session_.probe();
    result.available = !result.location.empty();
    return result;
}

} // namespace omvr::platform::openxr
