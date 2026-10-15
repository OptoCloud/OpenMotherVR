#pragma once

#include "core/diag/ports/log.h"
#include "core/stereo/types.h"
#include "game/addresses.h"
#include "game/layout.h"

#include <cstdint>

// The game's stereo mode stack (05 §4.4): mode 0 the 3D world with the HUD
// panel, mode 1 the virtual screen (menus, videos). Runs what the stereo
// mode policy decided, through the game's own push, pop and append. Game
// thread.
namespace omvr::game::stereo {

// The manager's mode and its stack, as a value.
core::stereo::ModeStackState mode_stack_state(const StereoManager& sm) noexcept;

// Runs `steps` on the stack of `sm` through Stereo::PushMode(int) and
// Stereo::PopMode(), the game's own. With debug options on, each change of
// the mode is logged, to follow which menus, videos and loading screens get
// the screen.
void apply_mode_steps(const AddressMap& addresses, core::diag::ports::Log& log, StereoManager& sm,
                      const core::stereo::ModeSteps& steps) noexcept;

// The game's mode-stack append (05 §3.2f), found by the installer and set
// once at attach, before the stereo update can run; null on a build without
// it. Read on the game thread.
struct ModeStackTargets {
    void* append = nullptr;
};

// Brings the stack count to the mode as `sync` says (05 §3.2f), appending
// through `targets` where that is the step.
core::stereo::StackSyncOutcome apply_stack_sync(const ModeStackTargets& targets, StereoManager& sm,
                                                core::stereo::StackSync sync) noexcept;

// The game's mode-stack append (spec 05 section 3.2f): thiscall on the
// stack array at StereoManager +0x68, one `const int*` argument, ret 4;
// grows the array through the game's allocator, raises the count and
// returns the new element, or null when the allocation failed. Set when
// the installer found it at its call in StereoPushMode; without it the
// count is raised only into storage the array already has.
using ModeStackAppendFn = std::int32_t* (__fastcall*)(void* array, void* edx, const std::int32_t* value);
// This build has the append routine.
inline bool has_mode_stack_append(const ModeStackTargets& targets) noexcept { return targets.append != nullptr; }

} // namespace omvr::game::stereo
