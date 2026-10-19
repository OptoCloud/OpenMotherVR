// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "util/base/error.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

// The one place that changes page protection and writes code or read-only
// data in this process (STYLEGUIDE 6, 12 §4).
namespace omvr::platform::win32 {

// In-process write to code or read-only data: switch the pages to
// PAGE_EXECUTE_READWRITE, write via WriteProcessMemory (which flushes the
// instruction cache), restore the previous protection. Pins the module
// first. Fails, after logging the Windows error (carried in the Error), if
// the pages could not be made writable or the write failed.
util::Result<void> write_memory(void* dst, std::span<const std::uint8_t> bytes,
                                core::diag::ports::Log& log) noexcept;

// Keep this DLL loaded for the rest of the process. Once a patch, vtable
// slot, detour or window procedure points into it, game code would call
// into unmapped memory after a FreeLibrary, so everything that installs one
// pins the module first. Idempotent and safe under the loader lock (the
// loader's lock is re-entrant for the thread that holds it).
void pin_this_module(core::diag::ports::Log& log) noexcept;

// VirtualProtect-swap a single pointer slot (a COM vtable entry, an import
// slot). The slot's previous value, which the replacement calls through, is
// stored in `original` (release) before the slot points at `replacement`:
// the slot may be shared by other objects and threads (every swapchain of a
// class shares its vtable), and a call through it must never find
// `original` unset. The page keeps execute rights while it is writable, in
// case code shares it. Returns false, with `original` and the slot
// unchanged, after logging `method` when the slot cannot be written.
bool replace_pointer(void** slot, void* replacement, std::atomic<void*>& original, const char* method,
                     core::diag::ports::Log& log) noexcept;

// Executable memory for code the mod generates: allocate_code returns
// `size` writable bytes (null on failure); seal_code makes them read-only
// and executable and flushes the instruction cache, after which they are
// never written again (no page stays writable and executable).
std::uint8_t* allocate_code(std::size_t size) noexcept;
void seal_code(std::uint8_t* code, std::size_t size) noexcept;
// Releases what allocate_code returned, for code that was never installed
// (nothing may still point into it). Null does nothing.
void free_code(std::uint8_t* code) noexcept;

} // namespace omvr::platform::win32
