#pragma once

#include "core/diag/ports/log.h"

namespace omvr::platform::win32 {

// Detours the function at `target` to `replacement` with MinHook, which
// relocates the overwritten prologue into a trampoline. `*trampoline`
// receives it before the detour goes live, so a call arriving at once
// already reaches the game's code through it. Pins the module. False,
// logged under `name`, when nothing was patched.
bool install_inline_hook(void* target, const void* replacement, void** trampoline, const char* name,
                         core::diag::ports::Log& log) noexcept;

} // namespace omvr::platform::win32
