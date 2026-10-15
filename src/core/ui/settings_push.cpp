// Pushes the settings into AI.exe's script/UI layer.
//
//   SettingsPush::push   <- FUN_10020870 @ 0x10020870 (996 bytes)
//
// Fires game-side script-variable setters for every setting so the in-game
// VR options panel reflects the current state.

#include "core/ui/settings_push.h"

#include "core/ui/runtime_menu.h"
#include "core/ui/ui_contract.h"

#include <string>
#include <utility>

namespace omvr::core::ui {

namespace {

// A help line: the action, a tab, the control.
constexpr char kHelpSeparator = '\t';

} // namespace

void SettingsPush::push(ports::UiHandle ui) noexcept {
    if (ui == ports::UiHandle{} || !deps_.vars.ready()) {
        // Once: the menu opens many times a session, and the reason does
        // not change.
        if (not_ready_logged_.first())
            deps_.log.line("[script_bridge] push_settings_to_game: callbacks not installed yet");
        return;
    }
    push_runtime_and_debug(ui);
    push_rendering(ui);
    // Seeds the record of what the render targets were built with.
    deps_.display_reset.reset_if_changed(deps_.settings);
    push_input(ui);
    push_controls_help(ui);
}

void SettingsPush::push_runtime_and_debug(ports::UiHandle ui) noexcept {
    const config::Settings& cfg = deps_.settings;
    // Whether the VR runtime object exists and its session is live.
    deps_.vars.set_bool(ui, script::kOptionsEnabled, deps_.runtime.session_active());
    deps_.vars.set_int(ui, script::kRuntime, std::to_underlying(ui_runtime_value(running_runtime(cfg, deps_.runtime))));
    deps_.vars.set_int(ui, script::kDebug, cfg.debug.debug ? 1 : 0);
    deps_.vars.set_bool(ui, script::kSkipIntro, cfg.debug.skip_startup_intro);
}

void SettingsPush::push_rendering(ports::UiHandle ui) noexcept {
    const config::RenderingSettings& r = deps_.settings.rendering;
    deps_.vars.set_int(ui, script::kMirrorView, std::to_underlying(r.mirror_view_type));
    deps_.vars.set_bool(ui, script::kMirrorViewSymmetric, r.symmetric_mirror_view);
    deps_.vars.set_float(ui, script::kPixelScale, r.pixel_scale);
    deps_.vars.set_bool(ui, script::kClipBlinding, r.clipping_blinders);
}

void SettingsPush::push_input(ports::UiHandle ui) noexcept {
    const config::InputSettings& in = deps_.settings.input;
    deps_.vars.set_bool(ui, script::kSnapRotation, in.snap_rotation);
    deps_.vars.set_int(ui, script::kSnapRotationSegments, in.snap_rotation_segments);
    deps_.vars.set_bool(ui, script::kInputRelativeWalking, in.vr_input_relative_walking);
    deps_.vars.set_bool(ui, script::kInputStrafing, in.vr_input_strafing);
    deps_.vars.set_bool(ui, script::kToggleGrips, in.toggle_grips);
    deps_.vars.set_bool(ui, script::kToggleCrouch, in.toggle_crouch);
}

void SettingsPush::push_controls_help(ports::UiHandle ui) noexcept {
    for (const char* line : deps_.schemes.control_help()) {
        const std::string_view s(line);
        const auto tab = s.find(kHelpSeparator);
        const std::string action(s.substr(0, tab));
        const std::string control(tab == std::string_view::npos ? std::string_view{} : s.substr(tab + 1));
        if (!deps_.vars.add_row(ui, script::kAddControlRow, action.c_str(), control.c_str())) break;
    }
}

} // namespace omvr::core::ui
