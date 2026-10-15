// OpenMotherVR-Provenance: clean
#pragma once

namespace omvr::platform::win32 {

// GetProcAddress: the export `name` of `module` (an HMODULE), or null.
void* proc_address(void* module, const char* name) noexcept;

// proc_address as a typed function pointer, or nullptr. The one cast from
// an export's address to its type (STYLEGUIDE 4); it goes through void* so
// it does not trip C4191 (FARPROC to an unrelated type).
template <class Fn>
Fn proc(void* module, const char* name) noexcept {
    return reinterpret_cast<Fn>(proc_address(module, name));
}

} // namespace omvr::platform::win32
