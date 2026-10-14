#pragma once

#include <cstddef>
#include <cstdint>

namespace mothervr::hook {

// In-process writes to AI.exe's code and read-only data: switch the pages to
// PAGE_EXECUTE_READWRITE, write via WriteProcessMemory, restore the previous
// protection. Used for code patches, call redirects and vtable/IAT slots.
//
// The original used the pseudo-handle 0xFFFFFFFF; ::GetCurrentProcess() is
// the same value.
//
// Each returns false, after logging GetLastError(), if the pages could not
// be made writable or the write failed.

// Arbitrary-size byte patch.
// From: the Protect/Write/Restore triples inlined through FUN_1001d6f0.
bool write_bytes(void* dst, const void* src, std::size_t n) noexcept;

// 4-byte write: function pointer, vtable slot, rel32 or disp32.
// From: FUN_1001d4d0 @ 0x1001d4d0 (the explicit helper) and dozens of inlined
// copies inside FUN_1001d6f0.
bool write_dword(void* dst, std::uint32_t value) noexcept;

} // namespace mothervr::hook
