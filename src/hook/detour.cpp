#include "detour.h"

#include "diag/log.h"
#include "memory_patch.h"

#include <MinHook.h>

#include <cstring>

namespace mothervr::hook {

namespace {

bool minhook_ready() noexcept {
    static const MH_STATUS status = MH_Initialize();
    return status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
}

} // namespace

bool detour(std::uint8_t* target, const void* replacement, std::span<const std::uint8_t> expected,
            void** original, const char* name) noexcept {
    if (!target || !original || std::memcmp(target, expected.data(), expected.size()) != 0) {
        diag::logf("[hook] {}: unexpected code at {}, not detoured", name, static_cast<void*>(target));
        return false;
    }
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
    diag::logf("[hook] {} detoured at {}", name, static_cast<void*>(target));
    return true;
}

} // namespace mothervr::hook
