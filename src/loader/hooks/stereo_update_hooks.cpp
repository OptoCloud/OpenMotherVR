#include "loader/hooks/stereo_update_hooks.h"

#include "core/stereo/ui_placement.h"
#include "game/stereo/follow_camera.h"
#include "game/stereo/stereo_objects.h"
#include "loader/context.h"
#include "loader/hooks/guard.h"

#include <cstdint>

namespace omvr::loader::hooks {

using game::DeviceState;
using game::StereoInterface;
using game::StereoManager;
using game::stereo::field;
using game::stereo::kFollowLookEnabled;
using game::stereo::load_device;
using game::stereo::load_state;

// Snap turn on the turn axis, recenter on LB+RB or Ctrl+Alt, game look only
// when snap is off or keyboard/mouse is used, scene visibility for
// cutscenes and loading. The game's own update (DK2 recenter, fade) is not
// run. From: FUN_1001f8a0.
int __fastcall stereo_update(StereoManager* self, void*, float /*dt*/) noexcept {
    auto& ctx = context();
    StereoInterface* device = load_device(*self);
    if (!device || load_state(*device) != DeviceState::Running) {
        if (ctx.follow.g_follow_active) field<std::uint8_t>(ctx.follow.g_follow, kFollowLookEnabled) = 1;
        return 0;
    }
    guarded("Stereo::Update", [&] { ctx.update.update_running(self, device); });
    return 0;
}

float* __cdecl place_virtual_screen(float* out, float size, float inv_aspect, float dist) noexcept {
    return core::stereo::place_virtual_screen(out, size, inv_aspect, dist);
}

float* __cdecl place_hud_panel(float* out, float size, float inv_aspect, float dist) noexcept {
    return core::stereo::place_hud_panel(out, size, inv_aspect, dist);
}

// Mode 0: 3D world with the HUD panel. Mode 1: the virtual screen (menus,
// videos). From: FUN_100201e0, FUN_10020220.
void __fastcall push_mode(StereoManager* self, void*, int mode) noexcept { context().modes.push(self, mode); }

void __fastcall pop_mode(StereoManager* self, void*) noexcept { context().modes.pop(self); }

// The control prompts ask whether movement is on the left stick; the game's
// own check reads the remapped bindings. Before the first virtualisation
// the move action has no recorded control, which the stock check answers
// true. From: FUN_10003100.
bool __fastcall is_move_on_left_stick(void*, void*) noexcept { return context().pad.move_on_left_stick(); }

} // namespace omvr::loader::hooks
