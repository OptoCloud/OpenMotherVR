#pragma once

#include <cstddef>

namespace omvr::platform::win32 {

// In-process writes to code and read-only data: switch the pages to
// PAGE_EXECUTE_READWRITE, write via WriteProcessMemory (which flushes the
// instruction cache), restore the previous protection. Pins the module
// first. Returns false, after logging GetLastError(), if the pages could not
// be made writable or the write failed.
bool write_memory(void* dst, const void* src, std::size_t n) noexcept;

// Keep this DLL loaded for the rest of the process. Once a patch, vtable
// slot, detour or window procedure points into it, game code would call
// into unmapped memory after a FreeLibrary, so everything that installs one
// pins the module first. Idempotent and safe under the loader lock (the
// loader's lock is re-entrant for the thread that holds it).
void pin_this_module() noexcept;

// VirtualProtect-swap a single pointer slot (a COM vtable entry). Returns
// the previous slot value (so the caller can stash it as the "real"
// function pointer), or null after logging `method` when the slot cannot be
// written.
void* replace_pointer(void** slot, void* replacement, const char* method) noexcept;

} // namespace omvr::platform::win32
