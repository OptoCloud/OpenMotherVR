#pragma once

#include <windows.h>

namespace mothervr::util {

// GetProcAddress as a typed function pointer, or nullptr. The cast goes
// through void* so it does not trip C4191 (FARPROC to an unrelated type).
template <class Fn>
Fn proc(HMODULE module, const char* name) noexcept {
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(::GetProcAddress(module, name)));
}

} // namespace mothervr::util
