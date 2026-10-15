#include "core/ui/commands.h"

#include "core/config/settings_store.h"
#include "core/config/settings_table.h"
#include "core/diag/logger.h"
#include "core/stereo/blinders.h"
#include "core/ui/runtime_menu.h"
#include "util/text/number_parse.h"

#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace omvr::core::ui {

namespace {

using util::parse_float;
using util::parse_int;
using util::parse_switch;

// Sets `field` and logs the change under its mothervr.json key path.
// Returns whether the value changed.
template <class T>
bool assign(T& field, T value, const char* key, std::string_view text) {
    if (field == value) return false;
    field = value;
    diag::logf("[script_bridge] {} changed to {} by command", key, text);
    return true;
}

bool assign_switch(bool& field, bool value, const char* key) {
    return assign(field, value, key, value ? "true" : "false");
}

} // namespace

void Commands::run(char* cmd) noexcept {
    try {
        // First token = command name (strtok_s mutates cmd in place), then
        // the value token; text after it is ignored. Mirrors the strtok-pair
        // pattern throughout FUN_100202e0.
        char* rest = nullptr;
        const char* name = strtok_s(cmd, " ", &rest);
        if (!name) return;
        const char* val = strtok_s(nullptr, " ", &rest);

        auto& cfg = config::current();
        bool accepted = true;
        bool quit     = false;
        auto& r = cfg.rendering;
        auto& in = cfg.input;

        if (std::strcmp(name, "mvr_vrruntime") == 0) {
            const auto n = parse_int(val);
            if (n && *n >= 0 && *n <= 2) {
                const auto runtime = runtime_from_ui(*n);
                assign(cfg.vr_runtime, runtime, "VRRuntime", config::to_string(runtime));
                // The new runtime takes effect on the next launch, so the
                // game quits, as in the original.
                quit = true;
            } else {
                accepted = false;
            }

        } else if (std::strcmp(name, "mvr_debug") == 0) {
            const auto v = parse_switch(val);
            accepted = v.has_value();
            if (accepted) assign_switch(cfg.debug.debug, *v, "DebugOptions");

        } else if (std::strcmp(name, "mvr_skipintro") == 0) {
            const auto v = parse_switch(val);
            accepted = v.has_value();
            if (accepted) assign_switch(cfg.debug.skip_startup_intro, *v, "SkipStartupIntro");

        } else if (std::strcmp(name, "mvr_mirrorview") == 0) {
            // 0 both eyes, 1 right eye, 2 left eye.
            const auto n = parse_int(val);
            accepted = n && *n >= 0 && *n <= static_cast<int>(config::MirrorViewType::LeftEye);
            if (accepted) {
                const auto m = static_cast<config::MirrorViewType>(*n);
                assign(r.mirror_view_type, m, "Rendering.MirrorViewType", config::to_string(m));
            }

        } else if (std::strcmp(name, "mvr_mirrorviewsym") == 0) {
            const auto v = parse_switch(val);
            accepted = v.has_value();
            if (accepted) assign_switch(r.symmetric_mirror_view, *v, "Rendering.SymmetricMirrorView");

        } else if (std::strcmp(name, "mvr_pixelscale") == 0) {
            // The range is the guard at FUN_100202e0 lines 183-185, which
            // reset an out-of-range value to 1.0; here it is refused.
            const auto v = parse_float(val);
            accepted = v && *v >= 0.5f && *v <= 4.0f;
            if (accepted)
                assign(r.pixel_scale, *v, "Rendering.PixelScale", std::format("{:.2f}", *v));

        } else if (std::strcmp(name, "mvr_clipblinding") == 0) {
            const auto v = parse_switch(val);
            accepted = v.has_value();
            if (accepted) {
                assign_switch(r.clipping_blinders, *v, "Rendering.ClippingBlinders");
                // Takes effect at once: the game reads the threshold through
                // a pointer patched into its head-in-wall check.
                stereo::set_clipping_blinders(r.clipping_blinders);
            }

        } else if (std::strcmp(name, "mvr_snaprotation") == 0) {
            const auto v = parse_switch(val);
            accepted = v.has_value();
            if (accepted) assign_switch(in.snap_rotation, *v, "Input.SnapRotation");

        } else if (std::strcmp(name, "mvr_snaprotationsegments") == 0) {
            // At least 2, then mapped down to 24/12/6/4/3/2 by sanitize().
            const auto n = parse_int(val);
            accepted = n && *n >= 2;
            if (accepted) {
                config::Settings mapped{};
                mapped.input.snap_rotation_segments = *n;
                config::sanitize(mapped);
                const int segments = mapped.input.snap_rotation_segments;
                assign(in.snap_rotation_segments, segments, "Input.SnapRotationSegments", std::to_string(segments));
            }

        } else if (std::strcmp(name, "mvr_inputrelativewalking") == 0) {
            const auto v = parse_switch(val);
            accepted = v.has_value();
            if (accepted) assign_switch(in.vr_input_relative_walking, *v, "Input.VRInputRelativeWalking");

        } else if (std::strcmp(name, "mvr_inputstrafing") == 0) {
            const auto v = parse_switch(val);
            accepted = v.has_value();
            if (accepted) assign_switch(in.vr_input_strafing, *v, "Input.VRInputStrafing");

        } else if (std::strcmp(name, "mvr_togglegrips") == 0) {
            const auto v = parse_switch(val);
            accepted = v.has_value();
            if (accepted) assign_switch(in.toggle_grips, *v, "Input.ToggleGrips");

        } else if (std::strcmp(name, "mvr_togglecrouch") == 0) {
            const auto v = parse_switch(val);
            accepted = v.has_value();
            if (accepted) assign_switch(in.toggle_crouch, *v, "Input.ToggleCrouch");

        } else {
            diag::logf("[script_bridge] unknown command {} ignored", name);
            return;
        }

        if (!accepted) {
            diag::logf("[script_bridge] {} {}: value not accepted, nothing changed", name, val ? val : "(none)");
            return;
        }
        config::sanitize(cfg);
        // Pixel scale and mirror changes need the game's targets rebuilt.
        display_reset_.reset_if_changed(cfg);

        // Persist the updated config to disk. Mirrors the FUN_1000d150() tail
        // call in FUN_100202e0 (an ofstream-based save wrapper). The original
        // also calls FUN_10003490() just before, but the result is unused,
        // likely an artefact of the compiler spilling ECX for a thiscall stub.
        // We omit it.
        config::save_current();

        // After the save: the new runtime is in the file before the game's
        // main loop reads the quit request.
        if (quit) signals_.request_exit();
    } catch (...) {
        // Nothing may escape into the game's dispatcher.
        diag::log_line("[script_bridge] command failed: out of memory");
    }
}

} // namespace omvr::core::ui
