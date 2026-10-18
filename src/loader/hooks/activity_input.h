// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/activity.h"
#include "core/ui/play_state.h"
#include "game/hands/hand_objects.h"
#include "game/stereo/stereo_objects.h"
#include "loader/context.h"

// The hands' and the body's activity signals (spec 21 §4.4, §12.1), read the
// same way by both hooks: all from v1's own state. Game thread.
namespace omvr::loader::hooks {

// This frame's signals for the player character `player` (null: none).
inline core::hands::ActivityInput activity_input(Context& ctx, game::StereoInterface& device,
                                                 void* player) noexcept {
    core::hands::ActivityInput in;
    in.enabled          = true;
    in.stereo_running   = game::stereo::load_state(device) == game::DeviceState::Running;
    in.world_mode       = ctx.manager.mode == game::kStereoModeWorld;
    in.player           = player != nullptr;
    in.alive            = player && game::hands::alive(player);
    in.follow_active    = ctx.follow.active();
    in.cutscene_or_fade = ctx.scene.cutscene_or_fade();
    in.menu_screen      = ctx.play.screen() != core::ui::Screen::None;
    in.interaction      = ctx.interactions.in_interaction();
    in.hiding           = ctx.interactions.hiding();
    return in;
}

} // namespace omvr::loader::hooks
