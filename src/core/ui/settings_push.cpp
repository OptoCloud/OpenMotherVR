// Pushes the settings into AI.exe's script/UI layer.
//
//   SettingsPush::push   <- FUN_10020870 @ 0x10020870 (996 bytes)
//
// Fires game-side script-variable setters for every setting so the in-game
// VR options panel reflects the current state.

#include "core/ui/settings_push.h"

#include "core/config/settings_store.h"
#include "core/diag/logger.h"
#include "core/stereo/stereo_device.h"
#include "core/ui/runtime_menu.h"

#include <string>
#include <string_view>
#include <utility>

namespace omvr::core::ui {

void SettingsPush::push(ports::UiHandle ui) noexcept {
    if (ui == ports::UiHandle{} || !vars_.ready()) {
        // Once: the menu opens many times a session, and the reason does
        // not change.
        if (!std::exchange(s_logged, true))
            diag::log_line("[script_bridge] push_settings_to_game: callbacks not installed yet");
        return;
    }

    const auto& cfg = config::current();

    // "setMotherVROptionsEnabled" mirrors the check:
    //   *(StereoManager + 0x04) != nullptr &&
    //   *(*(StereoManager + 0x04) + 0x04) != 0
    // which tests whether the VR runtime object exists and its session is live.
    vars_.set_bool (ui, "setMotherVROptionsEnabled",       stereo::vr_session_active());

    // VR backend selector and debug/startup flags
    vars_.set_int  (ui, "setMotherSetVRRuntime",           ui_runtime_value(running_runtime(cfg)));
    vars_.set_int  (ui, "setMotherDebug",                  cfg.debug.debug ? 1 : 0);
    vars_.set_bool (ui, "setMotherSkipIntro",              cfg.debug.skip_startup_intro);

    // Rendering: mirror mode, pixel scale, blinders
    vars_.set_int  (ui, "setMotherMirrorView",             static_cast<int>(cfg.rendering.mirror_view_type));
    vars_.set_bool (ui, "setMotherMirrorViewSym",          cfg.rendering.symmetric_mirror_view);
    vars_.set_float(ui, "setMotherPixelScale",             cfg.rendering.pixel_scale);
    vars_.set_bool (ui, "setMotherClipBlinding",           cfg.rendering.clipping_blinders);

    // Seeds the record of what the render targets were built with.
    display_reset_.reset_if_changed(cfg);

    // Input / snap rotation
    vars_.set_bool (ui, "setMotherSnapRotation",           cfg.input.snap_rotation);
    vars_.set_int  (ui, "setMotherSnapRotationSegments",   cfg.input.snap_rotation_segments);
    vars_.set_bool (ui, "setMotherInputRelativeWalking",   cfg.input.vr_input_relative_walking);
    vars_.set_bool (ui, "setMotherInputStrafing",          cfg.input.vr_input_strafing);
    vars_.set_bool (ui, "setMotherToggleGrips",            cfg.input.toggle_grips);
    vars_.set_bool (ui, "setMotherToggleCrouch",           cfg.input.toggle_crouch);

    // The controls help on the options page: one row per "Action\tControl"
    // line of the layout in use. Nothing when the game's row function is
    // unknown.
    for (const char* line : schemes_.control_help()) {
        const std::string_view s(line);
        const auto tab = s.find('\t');
        const std::string action(s.substr(0, tab));
        const std::string control(tab == std::string_view::npos ? std::string_view{} : s.substr(tab + 1));
        if (!vars_.add_row(ui, "addMotherViewControl", action.c_str(), control.c_str())) break;
    }
}

} // namespace omvr::core::ui
