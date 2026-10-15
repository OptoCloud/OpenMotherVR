#include "platform/win32/patching/inline_hook.h"

#include "core/diag/logger.h" // R: platform (A2): log through an injected Log&
#include "platform/win32/patching/memory.h"

#include <MinHook.h>

namespace omvr::platform::win32 {

namespace diag = core::diag;

namespace {

bool minhook_ready() noexcept {
    static const MH_STATUS status = MH_Initialize();
    return status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
}

} // namespace

bool install_inline_hook(void* target, const void* replacement, void** original, const char* name) noexcept {
    if (!minhook_ready()) {
        diag::logf("[hook] {}: MinHook failed to initialise, not detoured", name);
        return false;
    }
    pin_this_module();   // the detour jumps into this DLL from now on
    void* trampoline = nullptr;
    MH_STATUS status = MH_CreateHook(target, const_cast<void*>(replacement), &trampoline);
    if (status != MH_OK) {
        diag::logf("[hook] {}: MH_CreateHook: {}", name, MH_StatusToString(status));
        return false;
    }
    *original = trampoline;   // live before the first call can arrive
    status = MH_EnableHook(target);
    if (status != MH_OK) {
        diag::logf("[hook] {}: MH_EnableHook: {}", name, MH_StatusToString(status));
        MH_RemoveHook(target);
        *original = nullptr;
        return false;
    }
    diag::logf("[hook] {} detoured at {}", name, target);
    return true;
}

} // namespace omvr::platform::win32
