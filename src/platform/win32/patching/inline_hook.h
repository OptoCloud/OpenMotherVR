#pragma once

namespace omvr::platform::win32 {

// Detours the function at `target` to `replacement` with MinHook, which
// relocates the overwritten prologue into a trampoline. `*original`
// receives the trampoline before the detour goes live. Pins the module.
// False, logged under `name`, when nothing was patched.
bool install_inline_hook(void* target, const void* replacement, void** original, const char* name) noexcept;

} // namespace omvr::platform::win32
