// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"
#include "game/patcher.h"

#include <cstdint>

// The game sites of Interaction.CameraTakeovers (spec 23 §2.4, §4.2 (b)
// and (c)): the scripted turns of the follow camera, each a call the mod
// redirects to a replacement that runs the game's function or skips it.
// The group is checked as a whole before anything of it is written (12
// §4); with code that differs, or a build without its addresses, it stays
// off and is logged. Installed on the game thread when the key first asks
// for it ("Game", the default, installs nothing: 23 §4.3).
namespace omvr::game::sites {

// FollowSteer (23 §2.4): thiscall on the follow camera with a mode, a
// target position, a byte and a blend time; `ret 0x10`, no result.
using FollowSteerFn = void(__fastcall*)(void* follow, void* edx, std::int32_t mode, const float* target,
                                        std::uint32_t flag, float blend_time);
// The follow camera's re-align to the body heading (23 §2.4, method
// 0x2C6F0): thiscall with one stack argument (non-zero: at once); `ret 4`,
// no result.
using FollowRealignFn = void(__fastcall*)(void* follow, void* edx, std::uint32_t at_once);

// The replacements, one per call site, so each knows which takeover it is.
struct CameraTakeoverHooks {
    FollowSteerFn steal_camera_steer;          // the StealCamera start
    FollowSteerFn level_camera_exit_steer;     // the CameraResource stop
    FollowRealignFn level_camera_exit_realign; // the same, reset_player_camera_on_exit
    FollowRealignFn reset_realign;             // CameraResetRequest, mode 6
    FollowRealignFn state_entry_realign;       // the player state entry
};

// The game's functions the replacements call when the takeover is kept.
struct CameraTakeoverTargets {
    FollowSteerFn steer     = nullptr;
    FollowRealignFn realign = nullptr;
};

// Checks the group and redirects its five calls. Sets `out` before the
// first write. Game thread.
bool install_camera_takeover_sites(const GameImage& g, Patcher& p, const CameraTakeoverHooks& hooks,
                                   CameraTakeoverTargets& out) noexcept;

} // namespace omvr::game::sites
