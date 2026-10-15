#include "platform/openxr/runtime_probe.h"

#include "core/diag/logger.h" // R: vr (A1): log through an injected Log&
#include "platform/openxr/session.h"
#include "platform/win32/patching/pe_image.h"
#include "platform/win32/process_info.h"
#include "platform/win32/text.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace omvr::platform::openxr {

namespace {

namespace diag {
using core::diag::log_line;
using core::diag::logf;
using win32::utf8;
} // namespace diag
using win32::pe_machine;
using win32::process_elevated;

// ---- Runtime discovery --------------------------------------------------------

std::filesystem::path active_runtime(REGSAM view) {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Khronos\\OpenXR\\1", 0,
                        KEY_READ | view, &key) != ERROR_SUCCESS) {
        return {};
    }
    // A path of whatever length the registry holds (09 §1): ask for the size
    // first instead of assuming MAX_PATH.
    DWORD type = 0, size = 0;
    LSTATUS st = ::RegQueryValueExW(key, L"ActiveRuntime", nullptr, &type, nullptr, &size);
    std::wstring value;
    if (st == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 0) {
        value.resize(size / sizeof(wchar_t) + 1);   // room for a missing terminator
        DWORD got = static_cast<DWORD>(value.size() * sizeof(wchar_t));
        st = ::RegQueryValueExW(key, L"ActiveRuntime", nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &got);
    }
    ::RegCloseKey(key);
    if (st != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return {};
    value.resize(::wcsnlen(value.c_str(), value.size()));
    // An expandable value names its folder through environment references
    // (%ProgramFiles%...); the loader expands them, so must we, or the
    // manifest is not found.
    if (type == REG_EXPAND_SZ && !value.empty()) {
        if (const DWORD n = ::ExpandEnvironmentStringsW(value.c_str(), nullptr, 0); n > 0) {
            std::wstring expanded(n, L'\0');
            const DWORD m = ::ExpandEnvironmentStringsW(value.c_str(), expanded.data(), n);
            if (m > 0 && m <= n) {
                expanded.resize(m - 1);
                value = std::move(expanded);
            }
        }
    }
    return value;
}

// A manifest is usable from this process if its runtime library is x86.
bool usable_from_x86(const std::filesystem::path& manifest) {
    std::error_code ec;
    if (manifest.empty() || !std::filesystem::exists(manifest, ec)) return false;
    try {
        std::ifstream f(manifest);
        const auto j = nlohmann::json::parse(f);
        const auto lib_utf8 = j.at("runtime").at("library_path").get<std::string>();
        std::filesystem::path lib{std::u8string(lib_utf8.begin(), lib_utf8.end())};
        if (lib.is_relative()) lib = manifest.parent_path() / lib;
        const WORD m = pe_machine(lib);
        diag::logf("openxr: manifest {} -> {} (machine 0x{:04X})", diag::utf8(manifest), diag::utf8(lib), m);
        return m == IMAGE_FILE_MACHINE_I386;
    } catch (const std::exception& e) {
        diag::logf("openxr: cannot read manifest {}: {}", diag::utf8(manifest), e.what());
        return false;
    }
}

// 32-bit manifests usually sit next to the 64-bit one with "32" in place of
// "64" in the name (steamxr_win64.json -> steamxr_win32.json,
// oculus_openxr_64.json -> oculus_openxr_32.json).
std::vector<std::filesystem::path> x86_siblings(const std::filesystem::path& manifest64) {
    std::vector<std::filesystem::path> out;
    std::wstring name = manifest64.filename().wstring();
    for (const auto& [from, to] : {std::pair{L"win64", L"win32"}, std::pair{L"x64", L"x86"},
                                   std::pair{L"64", L"32"}}) {
        if (const auto pos = name.rfind(from); pos != std::wstring::npos) {
            std::wstring n = name;
            n.replace(pos, std::wcslen(from), to);
            out.push_back(manifest64.parent_path() / n);
        }
    }
    return out;
}

} // namespace

void point_loader_at(const std::filesystem::path& manifest) noexcept {
    if (manifest.empty()) return;
    PFN_xrInitializeLoaderKHR init_loader = nullptr;
    if (XR_FAILED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                        reinterpret_cast<PFN_xrVoidFunction*>(&init_loader))) ||
        !init_loader) {
        diag::log_line("openxr: loader has no xrInitializeLoaderKHR; only XR_RUNTIME_JSON points it at the manifest");
        return;
    }
    const std::string path = diag::utf8(manifest);
    XrLoaderInitPropertyValueEXT value{"XR_RUNTIME_JSON", path.c_str()};
    XrLoaderInitInfoPropertiesEXT props{XR_TYPE_LOADER_INIT_INFO_PROPERTIES_EXT};
    props.propertyValueCount = 1;
    props.propertyValues     = &value;
    const XrResult r = init_loader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&props));
    // Either outcome is logged (09 §1): this is the first thing to check
    // when "OpenXR does not start" is reported from an elevated game.
    if (XR_FAILED(r)) {
        diag::logf("openxr: loader property not set: {}; only XR_RUNTIME_JSON points the loader at {}",
                   static_cast<int>(r), path);
    } else {
        diag::logf("openxr: loader directed to {} (initialisation property accepted)", path);
    }
}

// Hands the chosen manifest to the loader through the environment (this
// module's CRT copy and the process), only once OpenXR is the runtime that
// starts: a probe for a runtime the player did not choose must not leave the
// variable to the game's child processes (01 §5.2). The elevated line goes
// with it, since only then is the loader property the one that counts.
void set_runtime_environment(const std::filesystem::path& manifest) noexcept {
    if (manifest.empty()) return;
    _wputenv_s(L"XR_RUNTIME_JSON", manifest.c_str());
    static bool s_elevation_logged = false;
    if (!s_elevation_logged && process_elevated()) {
        s_elevation_logged = true;
        diag::log_line("openxr: the game runs elevated (as administrator); the loader ignores the environment, "
                       "only its initialisation property points it at the runtime");
    }
}

std::filesystem::path Session::probe() noexcept {
    try {
        // An explicit override wins, as with the loader itself.
        wchar_t* env = nullptr;
        if (_wdupenv_s(&env, nullptr, L"XR_RUNTIME_JSON") == 0 && env) {
            const std::filesystem::path set{env};
            std::free(env);
            if (!set.empty()) {
                // The player chose: left as it is, nothing else passed (09 §1).
                g_manifest.clear();
                diag::logf("openxr: available: {} (XR_RUNTIME_JSON set by the player)", diag::utf8(set));
                return set;
            }
        }
        // SteamVR and most runtimes' "make active" write only the 64-bit
        // view, so a different 32-bit entry is usually left over from a
        // runtime used before: prefer the 32-bit build of the one the user
        // made active, and take the 32-bit entry only when that has none.
        const auto m64 = active_runtime(KEY_WOW64_64KEY);
        const auto m32 = active_runtime(KEY_WOW64_32KEY);
        diag::logf("openxr: active runtime 64-bit {}, 32-bit {}",
                   m64.empty() ? "(none)" : diag::utf8(m64), m32.empty() ? "(none)" : diag::utf8(m32));
        std::filesystem::path chosen;
        const char* rule = "";
        if (usable_from_x86(m64)) {
            chosen = m64;
            rule = "the 64-bit registration";
        } else {
            for (const auto& m : x86_siblings(m64)) {
                if (usable_from_x86(m)) {
                    chosen = m;
                    rule = "the 32-bit sibling of the 64-bit registration";
                    break;
                }
            }
        }
        if (chosen.empty() && usable_from_x86(m32)) {
            chosen = m32;
            rule = "the 32-bit registration";
        }
        if (chosen.empty()) {
            if (m64.empty() && m32.empty()) diag::log_line("openxr: not available: no active runtime registered");
            else diag::log_line("openxr: not available: no active runtime has a 32-bit build");
            return {};
        }
        diag::logf("openxr: available: {} ({})", diag::utf8(chosen), rule);
        // Handed to the loader (environment and loader property) only when
        // OpenXR is the runtime that starts: init_system. Probing alone
        // changes nothing in the process (01 §5.2).
        g_manifest = chosen;
        return chosen;
    } catch (const std::exception& e) {
        diag::logf("openxr: probe failed: {}", e.what());
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
