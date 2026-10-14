#pragma once

#include <cstdint>

// What the game's scripts are doing, from the debug strings they pass to a
// retail no-op: AI.exe 0xe1bcc0 (`mov eax, ecx; ret 4`, a __thiscall taking
// a const char*, 683 call sites). Interactions (terminals, keypads, levers,
// hacking, cutting, suit changes, ladders) and hiding announce their start
// and end there. Later features read this state: d-pad and lever emulation
// and lean/hold-breath while hiding, turn blocking during interactions.
//
// Not in the original.
namespace mothervr::game {

enum class Interaction : std::uint8_t {
    None, Generic, SuitChange, Keypad, Hacking, Cutting, Terminal, Ladder, Rewire,
};

// Safe from any thread.
bool        in_interaction() noexcept;  // between start_interaction and its end
Interaction interaction() noexcept;     // the specific kind, when announced
bool        hiding() noexcept;          // in a locker or under a table

// Detours the tap at `tap` (the hook installer calls this). False when the
// code there is not the expected no-op; the state then stays empty.
bool install_event_tap(std::uint8_t* tap) noexcept;

} // namespace mothervr::game
