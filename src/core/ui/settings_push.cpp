// Pushes the settings into AI.exe's script/UI layer.
//
//   SettingsPush::push   <- FUN_10020870 @ 0x10020870 (996 bytes)
//
// Fires game-side script-variable setters for every setting so the in-game
// VR options panel reflects the current state.

#include "core/ui/settings_push.h"

#include "core/config/settings_table.h"
#include "core/ui/runtime_menu.h"
#include "core/ui/ui_contract.h"

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace omvr::core::ui {

namespace {

// A help line: the action, a tab, the control.
constexpr char kHelpSeparator = '\t';

// Pushes `row`'s value in `cfg` through the setter of its type; an enum as
// its integer, as the page's lists hold them.
void push_row(ports::ScriptVariables& vars, ports::UiHandle ui, const config::SettingRow& row,
              config::Settings& cfg) {
    // The table's push names are string literals, so data() ends in a NUL.
    const char* name = row.push.data();
    std::visit(
        [&](auto field) {
            const auto value = field(cfg);
            using T          = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<T, bool>) {
                vars.set_bool(ui, name, value);
            } else if constexpr (std::is_same_v<T, float>) {
                vars.set_float(ui, name, value);
            } else if constexpr (std::is_same_v<T, int>) {
                vars.set_int(ui, name, value);
            } else {
                vars.set_int(ui, name, std::to_underlying(value));
            }
        },
        row.field);
}

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
    push_pages(ui);
    push_controls_help(ui);
}

void SettingsPush::push_runtime_and_debug(ports::UiHandle ui) noexcept {
    const config::Settings& cfg = deps_.settings;
    // Whether the VR runtime object exists and its session is live.
    deps_.vars.set_bool(ui, script::kOptionsEnabled, deps_.runtime.session_active());
    deps_.vars.set_int(ui, script::kRuntime,
                       std::to_underlying(ui_runtime_value(running_runtime(cfg, deps_.runtime))));
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

void SettingsPush::push_pages(ports::UiHandle ui) noexcept {
    // A copy: the table's accessors take a mutable Settings.
    config::Settings cfg = deps_.settings;
    for (const config::SettingRow& row : config::settings_table()) {
        if (!row.push.empty()) push_row(deps_.vars, ui, row, cfg);
    }
    deps_.vars.set_float(ui, script::kWorldScaleSwitch, deps_.play_space.world_scale_switch().value_or(0.0f));
    const ports::CalibrationView calibration = deps_.play_space.height_calibration();
    deps_.vars.set_int(ui, script::kCalibrationState, std::to_underlying(calibration.state));
    deps_.vars.set_int(ui, script::kCalibrationSeconds, calibration.seconds_left);
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
