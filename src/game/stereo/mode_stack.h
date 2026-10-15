#pragma once

#include "game/layout.h"

#include <cstdint>

// The game's stereo mode stack: mode 0 the 3D world with the HUD panel,
// mode 1 the virtual screen (menus, videos).
namespace omvr::game::stereo {

// Stereo::PushMode(int) and Stereo::PopMode(), the game's own.
void game_push_mode(StereoManager* sm, int mode) noexcept;
void game_pop_mode(StereoManager* sm) noexcept;

// Leave mode 1 for the world. The game's pop does nothing with an empty
// stack, which is the state when the game entered mode 1 without a push and
// there was no storage to raise the count into (sync_mode_stack): push 0
// through the game instead, which runs its SetMode(0) and appends with its
// own allocator, then drop that entry so the stack is empty in mode 0.
void game_pop_to_world(StereoManager* sm) noexcept;

// The game's mode-stack append (spec 05 section 3.2f): thiscall on the
// stack array at StereoManager +0x68, one `const int*` argument, ret 4;
// grows the array through the game's allocator, raises the count and
// returns the new element, or null when the allocation failed. Set when
// the installer found it at its call in StereoPushMode; without it the
// count is raised only into storage the array already has.
using ModeStackAppendFn = std::int32_t* (__fastcall*)(void* array, void* edx, const std::int32_t* value);
void install_mode_stack_append(void* append) noexcept;
// Null when this build has no address for it. Set at attach, read on the
// game thread.
ModeStackAppendFn mode_stack_append() noexcept;

} // namespace omvr::game::stereo
