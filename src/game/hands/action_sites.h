// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"
#include "game/patcher.h"

#include <cstdint>

// The game sites of the player's actions and animations (spec 21 §2.7,
// §2.11, §20, r11): the start methods of the player's melee action and of
// the scripted player actions, the play-animation command's request and
// its counterpart, and the physics velocity helper. Each is an entry
// detour that calls the game's own function; installed only when the
// feature that needs it is on (21 §1: at the defaults nothing is hooked).
namespace omvr::game::hands {

// 21 §2.11: the start methods, __thiscall with no stack arguments and a
// plain ret. What they return (if anything) is passed back unchanged.
using ActionStartFn = std::uintptr_t(__fastcall*)(void* self, void* edx);
// 21 §2.11: the play request and its counterpart, __thiscall with two
// stack arguments, ret 8.
using PlayAnimationFn = std::uintptr_t(__fastcall*)(void* self, void* edx, void* first, void* second);
// 21 §2.7: SetBodyVelocities, __cdecl (physics object, linear velocity,
// angular velocity).
using SetBodyVelocitiesFn = std::uintptr_t(__cdecl*)(void* body, const float* linear, const float* angular);

// The functions of one shape per site: the replacements on the way in, the
// trampolines to the game's own on the way out.
struct ActionSiteFns {
    ActionStartFn melee_a                   = nullptr;
    ActionStartFn melee_b                   = nullptr;
    ActionStartFn melee_kill                = nullptr;
    ActionStartFn trap_break                = nullptr;
    ActionStartFn melee_intercept           = nullptr;
    ActionStartFn impact_standing           = nullptr;
    ActionStartFn trap_victim               = nullptr;
    PlayAnimationFn play_request            = nullptr;
    PlayAnimationFn play_counterpart        = nullptr;
    SetBodyVelocitiesFn set_body_velocities = nullptr;
};

// Which groups to install.
struct ActionSitesWanted {
    bool starts         = false; // the melee and scripted action starts (21 §8.2, §12.1)
    bool play_animation = false; // the play request and its counterpart, for the log (21 §18)
    bool velocities     = false; // the velocity helper, for the throw's launch log (21 §22 item 8)
};

// Which groups went in.
struct ActionSitesOutcome {
    bool melee          = false;
    bool actions        = false;
    bool play_animation = false;
    bool velocities     = false;
};

// Checks every site of a group (first bytes, and the vtable or method
// table slot that holds the function where the spec gives one) before
// writing any of it (12 §4); a group whose code differs, or whose
// addresses this build lacks, stays off and is logged. The trampolines go
// to `out` before each detour goes live.
ActionSitesOutcome install_action_sites(const GameImage& g, Patcher& p, const ActionSiteFns& hooks,
                                        const ActionSitesWanted& wanted, ActionSiteFns& out) noexcept;

} // namespace omvr::game::hands
