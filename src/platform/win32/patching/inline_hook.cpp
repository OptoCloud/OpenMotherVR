// OpenMotherVR-Provenance: clean
#include "platform/win32/patching/inline_hook.h"

#include "platform/win32/patching/memory.h"

#include <MinHook.h>

namespace omvr::platform::win32 {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kInitFailedLog   = "[hook] {}: MinHook failed to initialise, not detoured";
constexpr std::string_view kCreateFailedLog = "[hook] {}: MH_CreateHook: {}";
constexpr std::string_view kEnableFailedLog = "[hook] {}: MH_EnableHook: {}";
constexpr std::string_view kDetouredLog     = "[hook] {} detoured at {}";

// MinHook is initialised once, on the first detour, and never shut down:
// the detours stay for the rest of the process.
bool minhook_ready() noexcept {
    static const MH_STATUS status = MH_Initialize();
    return status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
}

} // namespace

bool install_inline_hook(void* target, const void* replacement, void** trampoline, const char* name,
                         ports::Log& log) noexcept {
    if (!minhook_ready()) {
        ports::logf(log, kInitFailedLog, name);
        return false;
    }
    pin_this_module(log); // the detour jumps into this DLL from now on
    void* created    = nullptr;
    MH_STATUS status = MH_CreateHook(target, const_cast<void*>(replacement), &created);
    if (status != MH_OK) {
        ports::logf(log, kCreateFailedLog, name, MH_StatusToString(status));
        return false;
    }
    *trampoline = created; // live before the first call can arrive
    status      = MH_EnableHook(target);
    if (status != MH_OK) {
        ports::logf(log, kEnableFailedLog, name, MH_StatusToString(status));
        MH_RemoveHook(target);
        *trampoline = nullptr;
        return false;
    }
    ports::logf(log, kDetouredLog, name, target);
    return true;
}

} // namespace omvr::platform::win32
