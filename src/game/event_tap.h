#pragma once

#include "core/diag/ports/log.h"

#include <cstdint>

namespace omvr::game {

// The script event tap: a retail no-op, AI.exe 0xe1bcc0 (`mov eax, ecx;
// ret 4`), that scripts pass debug strings to (05 §12). __thiscall(this,
// const char*), callee pops the argument: __fastcall with the unused edx
// matches it.
using TapFn = void*(__fastcall*)(void* self, void* edx, const char* text);

// Detours the tap at `tap` to `replacement`; `trampoline` receives the way
// back into the game's code. False when the code there is not the expected
// no-op; `trampoline` then stays empty.
bool install_event_tap(std::uint8_t* tap, TapFn replacement, TapFn& trampoline,
                       core::diag::ports::Log& log) noexcept;

// The tap's code is shared: the game's linker folded other functions with
// the same body into it, some of which Scaleform calls through a vtable with
// an argument that is not a string. Every event name is a literal in the
// game's image, so only pointers into it are read.
bool in_game_image(const char* p) noexcept;

} // namespace omvr::game
