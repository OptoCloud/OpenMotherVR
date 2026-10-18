#include "loader/hooks/stereo_update_hooks.h"

#include "core/stereo/mode_policy.h"
#include "core/stereo/tuning.h"
#include "core/stereo/ui_placement.h"
#include "game/constants.h"
#include "game/input/input_devices.h"
#include "game/stereo/mode_stack.h"
#include "game/stereo/pov.h"
#include "game/stereo/stereo_objects.h"
#include "loader/context.h"
#include "loader/hooks/body_hooks.h"
#include "loader/hooks/guard.h"
#include "loader/hooks/hud_hooks.h"
#include "loader/hooks/pad_hooks.h"
#include "loader/hooks/stereo_device_hooks.h"

#include <algorithm>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

using core::stereo::Matrix44;
using core::stereo::ModePolicy;
using core::stereo::StereoMode;
using core::stereo::UpdateInput;
using core::stereo::UpdateOutput;
using game::DeviceState;
using game::StereoInterface;
using game::StereoManager;
using game::stereo::load_device;
using game::stereo::load_state;
namespace abi = game::stereo::abi;

StereoMode mode_of(const StereoManager& m) noexcept { return static_cast<StereoMode>(m.mode); }

// 05 §3.2b, c, e: the pad's recenter chord, activity and turn axis.
void read_pad(Context& ctx, void* pad, UpdateInput& in) noexcept {
    in.pad          = true;
    in.lb           = stock_pad_button(pad, game::kPadLB);
    in.rb           = stock_pad_button(pad, game::kPadRB);
    in.r3           = stock_pad_button(pad, game::kPadRStick);
    in.pad_activity = merged_pad_activity(pad);
    // The turn is the player map's analog action; its virtual code follows
    // the map's digital count (0x12C on the shipped map), so it is derived.
    in.turn = merged_pad_axis(pad, ctx.pad.analog_virtual_code(game::kPlayerMap, game::player_action::kTurn));
    // Spec 22 §5: the movement the comfort vignette follows.
    in.move_x =
        merged_pad_axis(pad, ctx.pad.analog_virtual_code(game::kPlayerMap, game::player_action::kMoveX));
    in.move_y =
        merged_pad_axis(pad, ctx.pad.analog_virtual_code(game::kPlayerMap, game::player_action::kMoveY));
}

// 05 §3.2b, e: the keyboard chord and keyboard or mouse use.
void read_keyboard_and_mouse(const game::AddressMap& addresses, void* keyboard, void* mouse,
                             UpdateInput& in) noexcept {
    in.keyboard_and_mouse = true;
    in.ctrl               = game::input::key_down(addresses, keyboard, game::kScanLCtrl);
    in.alt                = game::input::key_down(addresses, keyboard, game::kScanLAlt);
    in.key_or_mouse_activity =
        game::input::any_key(addresses, keyboard) || game::input::mouse_active(addresses, mouse);
}

// 22 §13: right Ctrl + right Shift + F10, held, turns VR off for the
// session; read with the game's key test while the keyboard and mouse
// devices exist.
void check_vr_off(Context& ctx) noexcept {
    bool held      = false;
    void* keyboard = ctx.input_manager.keyboard_device();
    if (ctx.settings.current().controls.vr_off_hotkey && keyboard && ctx.input_manager.mouse_device()) {
        held = game::input::key_down(ctx.addresses, keyboard, game::kScanRCtrl) &&
               game::input::key_down(ctx.addresses, keyboard, game::kScanRShift) &&
               game::input::key_down(ctx.addresses, keyboard, game::kScanF10);
    }
    if (ctx.vr_off.update(held, ctx.clock.now())) ctx.stereo.request_vr_off();
}

// Runs under guarded: the runtime's headset_in_use may throw.
UpdateInput gather(Context& ctx, StereoManager& m, StereoInterface& device) {
    UpdateInput in;
    void* pad      = ctx.input_manager.pad_device();
    void* keyboard = ctx.input_manager.keyboard_device();
    void* mouse    = ctx.input_manager.mouse_device();
    in.now         = ctx.clock.now();
    if (pad) read_pad(ctx, pad, in);
    if (keyboard && mouse) read_keyboard_and_mouse(ctx.addresses, keyboard, mouse, in);
    in.follow_active = ctx.follow.active();
    if (in.follow_active) in.follow_yaw = ctx.follow.yaw();
    in.mode = mode_of(m);
    // Spec 22 §4.4: the script events' interaction state.
    in.in_interaction   = ctx.interactions.in_interaction();
    in.interaction_kind = ctx.interactions.interaction();
    in.breathing_game   = ctx.interactions.breathing_game();
    // Asked only while a recenter waits for the headset, as the runtime
    // reads a fresh pose for it.
    auto* runtime     = game::load_backend(device);
    in.headset_in_use = ctx.update.recenter_pending() && runtime && runtime->headset_in_use();
    return in;
}

// Re-places the menu screen in front of the current view (05 §3.2e).
void place_menu_screen(Context& ctx, StereoManager& m) noexcept {
    const Matrix44 quad = core::stereo::virtual_screen_quad(game::stereo::view_matrix(ctx.addresses),
                                                            core::stereo::kMenuScreenInvAspect);
    std::ranges::copy(quad, m.game_matrix);
}

// Rule f (05 §3.2f): the stack count follows the mode.
void sync_mode_stack(Context& ctx, StereoManager& m) noexcept {
    const auto sync = ModePolicy::sync_mode_stack(
        game::stereo::mode_stack_state(m), game::stereo::has_mode_stack_append(ctx.targets.mode_stack));
    ctx.modes.report(game::stereo::apply_stack_sync(ctx.targets.mode_stack, m, sync));
}

// Dev. Debug Options: the scene visibility as the stereo update finds it
// (`found`: what the game or the head-in-wall fade wrote) and as it leaves
// it, each logged when it leaves clear and when it comes back, with what the
// update knows. Game thread, once per change.
void trace_visibility(Context& ctx, bool found, float visibility) noexcept {
    using Change        = core::stereo::VisibilityTrace::Change;
    auto& trace         = found ? ctx.found_visibility_trace : ctx.final_visibility_trace;
    const char* where   = found ? "found at the stereo update" : "left for the frame";
    const bool cutscene = ctx.scene.cutscene_or_fade();
    const bool loading  = ctx.update.loading_blackout();
    const char* camera  = ctx.follow.active() ? "player" : "taken";
    switch (trace.update(visibility)) {
        case Change::Darkened:
            core::diag::ports::logf(ctx.log,
                                    "[diag] scene visibility {:.2f} {}; cutscene {}, loading {}, camera {}",
                                    visibility, where, cutscene, loading, camera);
            break;
        case Change::Cleared:
            core::diag::ports::logf(ctx.log, "[diag] scene visibility {} clear again (lowest {:.2f})", where,
                                    trace.lowest());
            break;
        case Change::None: break;
    }
}

void apply(Context& ctx, StereoManager& m, StereoInterface& device, const UpdateOutput& out) {
    const bool trace = ctx.log.debug_enabled();
    if (trace) trace_visibility(ctx, true, m.game_64);
    if (out.yaw) ctx.follow.set_yaw(*out.yaw);
    if (out.look_enabled) {
        ctx.follow.set_look_enabled(*out.look_enabled);
        ctx.follow.trace_snap(ctx.log, ctx.clock.now(), out.last_snap, out.keyboard_mouse);
    }
    if (out.recenter) game::stereo::recenter(m, device);
    if (out.place_screen) place_menu_screen(ctx, m);
    sync_mode_stack(ctx, m);
    if (out.scene_visibility) m.game_64 = *out.scene_visibility;
    if (trace) trace_visibility(ctx, false, m.game_64);
    // Spec 22 §4.3, §5, §15: to the render thread's slot 6.
    ctx.comfort.publish_target(out.vignette_target);
    if (out.snap_fade) ctx.comfort.note_snap_step();
    if (out.turning_changed) core::stereo::log_turning(ctx.log, *out.turning_changed);
    if (out.block_changed) core::stereo::log_turn_block(ctx.log, *out.block_changed);
}

// 22 §6: this frame's play-space settings, latched before any pose of the
// frame (a play mode change asks for a recenter, at which it applies, 6.1),
// and a pending height calibration (6.2), which reads the head only while
// it waits.
void update_play_space(Context& ctx, StereoInterface& device) {
    if (ctx.play_space.begin_frame(core::stereo::play_space_settings(ctx.settings.current())))
        ctx.update.request_recenter();
    if (!ctx.calibration.pending()) return;
    core::stereo::CalibrationReading reading;
    if (auto* runtime = game::load_backend(device)) {
        reading.has_floor = runtime->has_floor();
        reading.in_use    = runtime->headset_in_use();
        if (reading.in_use) reading.head_height_m = runtime->head_height_above_floor();
    }
    store_standing_height(ctx.calibration.update(ctx.clock.now(), reading));
}

float* place_quad(float* out, const Matrix44& quad) noexcept {
    std::ranges::copy(quad, out);
    return out;
}

} // namespace

// Snap turn on the turn axis, recenter on LB+RB or Ctrl+Alt, game look only
// when snap is off or keyboard/mouse is used, scene visibility for
// cutscenes and loading. The game's own update (DK2 recenter, fade) is not
// run. From: FUN_1001f8a0.
int __fastcall stereo_update(StereoManager* self, void*, float /*dt*/) noexcept {
    auto& ctx               = context();
    StereoInterface* device = load_device(*self);
    if (!device || load_state(*device) != DeviceState::Running) {
        // 05 §3.2a: without a running headset only the game's own look is
        // kept on.
        if (ctx.follow.active()) ctx.follow.set_look_enabled(true);
        ctx.comfort.publish_target(0.0f); // spec 22 §5: no vignette without a running device
        return 0;
    }
    check_vr_off(ctx);
    guarded("Stereo::Update", [&] {
        // 03 §2.2, §6.8: the runtime moved its tracking space; the
        // reference is retaken once the headset is in use.
        if (auto* runtime = game::load_backend(*device); runtime && runtime->tracking_space_changed())
            ctx.update.request_recenter();
        update_play_space(ctx, *device);
        // 22 §8.2, §15: the two-handed recenter, asked for on the render
        // thread, taken here as a manual recenter.
        if (ctx.gestures.take_recenter()) ctx.update.request_recenter();
        const UpdateOutput out = ctx.update.run(gather(ctx, *self, *device));
        apply(ctx, *self, *device, out);
        // 23 §7, 22 §8.3: the HUD options' game-thread work.
        hud_on_stereo_update(out.yaw.has_value() || out.recenter);
        // 21 §10 to §12: body visibility, real crouch, the control hand-back.
        body_on_stereo_update(*device);
    });
    return 0;
}

float* __cdecl place_virtual_screen(float* out, float, float inv_aspect, float) noexcept {
    return place_quad(
        out, core::stereo::virtual_screen_quad(game::stereo::view_matrix(context().addresses), inv_aspect));
}

float* __cdecl place_hud_panel(float* out, float, float inv_aspect, float) noexcept {
    return place_quad(out, hud_panel_for_frame(game::stereo::view_matrix(context().addresses), inv_aspect));
}

// Mode 0: 3D world with the HUD panel. Mode 1: the virtual screen (menus,
// videos). From: FUN_100201e0, FUN_10020220.
void __fastcall push_mode(StereoManager* self, void*, int mode) noexcept {
    auto& ctx = context();
    game::stereo::apply_mode_steps(ctx.addresses, ctx.log, *self,
                                   ModePolicy::push(mode_of(*self), static_cast<StereoMode>(mode)));
}

void __fastcall pop_mode(StereoManager* self, void*) noexcept {
    auto& ctx = context();
    game::stereo::apply_mode_steps(ctx.addresses, ctx.log, *self, ModePolicy::pop(mode_of(*self)));
}

// The control prompts ask whether movement is on the left stick; the game's
// own check reads the remapped bindings. Before the first virtualisation
// the move action has no recorded control, which the stock check answers
// true. From: FUN_10003100.
bool __fastcall is_move_on_left_stick(void*, void*) noexcept { return context().pad.move_on_left_stick(); }

static_assert(std::is_convertible_v<decltype(&stereo_update), abi::Update>);
static_assert(std::is_convertible_v<decltype(&place_virtual_screen), abi::BuildUiQuad>);
static_assert(std::is_convertible_v<decltype(&place_hud_panel), abi::BuildUiQuad>);
static_assert(std::is_convertible_v<decltype(&push_mode), abi::PushMode>);
static_assert(std::is_convertible_v<decltype(&pop_mode), abi::PopMode>);
static_assert(std::is_convertible_v<decltype(&is_move_on_left_stick), abi::BoolNoArgs>);

} // namespace omvr::loader::hooks
