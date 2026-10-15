// The real d3d11.dll behind the proxy (01 §1.1, §1.2: Windows 11 loads a
// local d3d11.dll from the game's folder).

#include "platform/d3d11/system_d3d11.h"

#include "platform/d3d11/constants.h"
#include "platform/win32/proc.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>

namespace omvr::platform::d3d11 {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kNoSystemDirLog    = "proxy: GetSystemDirectory failed";
constexpr std::string_view kLoadFailedLog     = "proxy: LoadLibraryEx failed err=0x{:08X}";
constexpr std::string_view kLoadedSelfLog     = "proxy: LoadLibrary returned SELF, aborting";
constexpr std::string_view kLoadedLog         = "proxy: real d3d11 loaded";
constexpr std::string_view kResolvedSelfLog   = "proxy: GetProcAddress returned SELF or null, aborting";
constexpr std::string_view kResolvedLog       = "proxy: D3D11CreateDeviceAndSwapChain resolved";
constexpr std::string_view kMissingExportLog  = "proxy: real d3d11 lacks export {}";
constexpr std::string_view kForwardSummaryLog = "proxy: {} pass-through exports resolved, {} missing";

using SystemPath = std::array<wchar_t, MAX_PATH>;

// The absolute path of the system's d3d11.dll. Passing just "d3d11.dll" to
// LoadLibrary matches by basename in the loaded-modules list and would
// return OUR handle, causing infinite recursion on the first forward.
std::optional<SystemPath> system_d3d11_path() noexcept {
    SystemPath path{};
    const UINT n = ::GetSystemDirectoryW(path.data(), MAX_PATH);
    // The suffix, its terminator included, must fit after the directory.
    if (n == 0 || n + std::size(kSystemD3d11) >= MAX_PATH) return std::nullopt;
    std::ranges::copy(kSystemD3d11, path.begin() + n);
    return path;
}

} // namespace

bool SystemD3d11::load(std::span<void*> forward, std::span<const char* const> names,
                       ports::Log& log) noexcept {
    ::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&kSystemD3d11), &self_);

    const auto path = system_d3d11_path();
    if (!path) {
        log.line(kNoSystemDirLog);
        return false;
    }
    real_module_ = ::LoadLibraryExW(path->data(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!real_module_) {
        ports::logf(log, kLoadFailedLog, ::GetLastError());
        return false;
    }
    if (real_module_ == self_) {
        log.line(kLoadedSelfLog);
        real_module_ = nullptr;
        return false;
    }
    log.line(kLoadedLog);

    create_device_and_swapchain_ =
        win32::proc<CreateDeviceAndSwapChainFn>(real_module_, kCreateDeviceAndSwapChainExport);
    if (!create_device_and_swapchain_ ||
        create_device_and_swapchain_ ==
            win32::proc<CreateDeviceAndSwapChainFn>(self_, kCreateDeviceAndSwapChainExport)) {
        log.line(kResolvedSelfLog);
        create_device_and_swapchain_ = nullptr;
        return false;
    }
    log.line(kResolvedLog);

    // A missing name leaves its slot null; calling that export then faults
    // on the jmp, which is louder than any guessed fallback would be (we
    // can't fake a return without knowing the callee's stack cleanup).
    std::size_t missing = 0;
    for (std::size_t i = 0; i < forward.size(); ++i) {
        forward[i] = win32::proc<void*>(real_module_, names[i]);
        if (!forward[i]) {
            ports::logf(log, kMissingExportLog, names[i]);
            ++missing;
        }
    }
    ports::logf(log, kForwardSummaryLog, forward.size() - missing, missing);
    return true;
}

void SystemD3d11::shutdown() noexcept {
    if (real_module_) {
        ::FreeLibrary(real_module_);
        real_module_ = nullptr;
    }
}

} // namespace omvr::platform::d3d11
