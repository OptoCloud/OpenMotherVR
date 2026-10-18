// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/activity.h"
#include "core/ui/play_state.h"
#include "game/hands/hand_objects.h"
#include "game/stereo/stereo_objects.h"
#include "game/code_address.h"
#include "loader/context.h"

#include <cstdint>

// The hands' and the body's activity signals (spec 21 §4.4, §12.1), read the
// same way by both hooks: all from v1's own state. Game thread.
namespace omvr::loader::hooks {

// 21 §12.1 (r11): a player action episode runs. The current item, which
// ends it, is read only while one runs.
inline bool player_action_signal(Context& ctx, void* player, bool follow_active) noexcept {
    if (!ctx.hands.player_action_wanted()) return false;
    std::uintptr_t item = 0;
    if (player && ctx.targets.hands.current_item) {
        if (void* component = game::hands::weapon_component(player))
            item = game::address_of(game::hands::current_weapon(ctx.targets.hands, component).object);
    }
    return ctx.hands.player_action(item, follow_active, ctx.clock.now());
}

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
    in.player_action    = player_action_signal(ctx, player, in.follow_active);
    return in;
}

} // namespace omvr::loader::hooks
