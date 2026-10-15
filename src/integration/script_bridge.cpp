// Bidirectional bridge between MotherVR's config state and AI.exe's
// script/UI layer. Two functions are reconstructed here:
//
//   push_settings_to_game()   <- FUN_10020870 @ 0x10020870 (996 bytes)
//   handle_mvr_command()      <- FUN_100202e0 @ 0x100202e0 (1418 bytes)
//
// push_settings_to_game() fires game-side script-variable setters for every
// config field so the in-game VR options panel reflects the current state.
// handle_mvr_command() is installed into AI.exe's console-command dispatch and
// intercepts "mvr_*" tokens, writing them back into config::current() and
// saving to disk.

#include "script_bridge.h"

#include "config/config.h"
#include "diag/crash_log.h"
#include "diag/log.h"
#include "input/vr_controls.h"
#include "runtime/vr_detect.h"
#include "stereo/game_hooks.h"
#include "stereo/stereo_device.h"

#include <charconv> // from_chars
#include <cstring>  // strtok_s, strncmp, strcmp, strlen
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>  // exchange

namespace mothervr::integration {

// ---- Globals ----------------------------------------------------------------

PFN_FireStr2   g_fire_str2    = nullptr;
PFN_FireBool   g_fire_bool    = nullptr;
PFN_FireInt    g_fire_int     = nullptr;
PFN_FireFloat  g_fire_float   = nullptr;
PFN_CmdHandler g_original_cmd = nullptr;
PFN_MenuMethod g_original_menu_open = nullptr;
std::uint8_t*  g_exit_request       = nullptr;

bool          g_vr_session_active    = false;
std::uint8_t*  g_display_reset_flag = nullptr;

// ---- Helpers ----------------------------------------------------------------

namespace {

// True when the script-variable setters every push needs have been
// installed by the hook installer, so early or late calls are safe.
// g_fire_str2 (the controls help rows) is checked where it is used.
bool callbacks_ready() noexcept {
    return g_fire_bool && g_fire_int && g_fire_float;
}

// The menu's runtime entries: 0 none, 1 the original's "Oculus" slot, now
// "OpenXR" (the UI patch relabels it; the original's LibOVR backend is not
// reconstructed, and Meta's runtime is an OpenXR runtime), 2 OpenVR.
int ui_runtime_value(config::VRRuntime r) noexcept {
    switch (r) {
        case config::VRRuntime::OpenXR:
        case config::VRRuntime::Oculus: return 1;
        case config::VRRuntime::OpenVR: return 2;
        default:                        return 0;
    }
}

// The runtime this run drives: None when the chosen one failed to start, the
// fallback when the stored one was not available. The menu then offers the
// stored runtime (a retry) and the other one (the way back), where showing
// the stored choice would hide that VR is not running.
config::VRRuntime running_runtime(const config::Config& cfg) noexcept {
    if (cfg.vr_runtime == config::VRRuntime::None || stereo::runtime_failed()) return config::VRRuntime::None;
    switch (runtime::last_result().chosen) {
        case runtime::detect_result::Chosen::OpenVR: return config::VRRuntime::OpenVR;
        case runtime::detect_result::Chosen::OpenXR: return config::VRRuntime::OpenXR;
        default:                                     return config::VRRuntime::None;
    }
}

config::VRRuntime runtime_from_ui(int v) noexcept {
    return v == 1 ? config::VRRuntime::OpenXR
         : v == 2 ? config::VRRuntime::OpenVR
                  : config::VRRuntime::None;
}

// The settings the game's render targets were last built with. The original
// raises the reset flag on every menu open; a rebuild stalls the game for
// about 0.2 s, so it is raised only when one of these actually changed.
struct TargetSettings {
    float                  pixel_scale;
    config::MirrorViewType mirror_view_type;
    bool                   symmetric_mirror_view;
    bool operator==(const TargetSettings&) const = default;
};

TargetSettings target_settings(const config::Config& cfg) noexcept {
    return {cfg.rendering.pixel_scale, cfg.rendering.mirror_view_type, cfg.rendering.symmetric_mirror_view};
}

void reset_display_if_changed(const config::Config& cfg) noexcept {
    static TargetSettings built = target_settings(cfg); // first call: what the targets were made with
    const TargetSettings now = target_settings(cfg);
    if (now == built) return;
    const TargetSettings was = std::exchange(built, now);
    // With no VR this run there are no stereo targets to resize, and a
    // rebuild would only freeze the picture.
    if (!g_display_reset_flag || running_runtime(cfg) == config::VRRuntime::None) return;
    *g_display_reset_flag = 1;
    diag::logf("[script_bridge] display reset raised; changed:{}{}{}",
               now.pixel_scale != was.pixel_scale ? " Rendering.PixelScale" : "",
               now.mirror_view_type != was.mirror_view_type ? " Rendering.MirrorViewType" : "",
               now.symmetric_mirror_view != was.symmetric_mirror_view ? " Rendering.SymmetricMirrorView" : "");
}

// A command's value: a decimal integer with an optional sign and nothing
// after it in the token. Anything else is not a value (spec 06 §4.2).
std::optional<int> parse_int(const char* s) noexcept {
    if (!s) return std::nullopt;
    if (*s == '+') ++s;
    const char* end = s + std::strlen(s);
    int v = 0;
    const auto [p, ec] = std::from_chars(s, end, v);
    if (ec != std::errc{} || p != end || p == s) return std::nullopt;
    return v;
}

// A decimal number with '.' as the decimal point whatever the system
// locale (the menu sends "1.5" and "2").
std::optional<float> parse_float(const char* s) noexcept {
    if (!s) return std::nullopt;
    if (*s == '+') ++s;
    const char* end = s + std::strlen(s);
    float v = 0.0f;
    const auto [p, ec] = std::from_chars(s, end, v, std::chars_format::fixed);
    if (ec != std::errc{} || p != end || p == s) return std::nullopt;
    return v;
}

// 0 or 1.
std::optional<bool> parse_switch(const char* s) noexcept {
    const auto n = parse_int(s);
    if (!n || (*n != 0 && *n != 1)) return std::nullopt;
    return *n == 1;
}

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

// ---- push_settings_to_game --------------------------------------------------
//
// Pushes the entire live config into the game's script-variable layer, on the
// menu object `ui`. Corresponds to FUN_10020870.

void push_settings_to_game(void* ui) noexcept {
    // Menus open (front end, pause, loading screen) after the game has set
    // up whatever exception handling of its own it installs, and now and
    // then during play: a cheap moment, on the game thread, to make sure a
    // crash still reaches the crash record.
    diag::reassert_crash_log();

    if (!ui || !callbacks_ready()) {
        // Once: the menu opens many times a session, and the reason does
        // not change.
        static bool s_logged = false;
        if (!std::exchange(s_logged, true))
            diag::log_line("[script_bridge] push_settings_to_game: callbacks not installed yet");
        return;
    }

    const auto& cfg = config::current();

    // "setMotherVROptionsEnabled" mirrors the check:
    //   *(StereoManager + 0x04) != nullptr &&
    //   *(*(StereoManager + 0x04) + 0x04) != 0
    // which tests whether the VR runtime object exists and its session is live.
    g_fire_bool (ui, nullptr, "setMotherVROptionsEnabled",       g_vr_session_active ? 1 : 0);

    // VR backend selector and debug/startup flags
    g_fire_int  (ui, nullptr, "setMotherSetVRRuntime",           ui_runtime_value(running_runtime(cfg)));
    g_fire_int  (ui, nullptr, "setMotherDebug",                  cfg.debug.debug ? 1 : 0);
    g_fire_bool (ui, nullptr, "setMotherSkipIntro",              cfg.debug.skip_startup_intro ? 1 : 0);

    // Rendering: mirror mode, pixel scale, blinders
    g_fire_int  (ui, nullptr, "setMotherMirrorView",             static_cast<int>(cfg.rendering.mirror_view_type));
    g_fire_bool (ui, nullptr, "setMotherMirrorViewSym",          cfg.rendering.symmetric_mirror_view ? 1 : 0);
    g_fire_float(ui, nullptr, "setMotherPixelScale",             cfg.rendering.pixel_scale);
    g_fire_bool (ui, nullptr, "setMotherClipBlinding",           cfg.rendering.clipping_blinders ? 1 : 0);

    // Seeds the record of what the render targets were built with.
    reset_display_if_changed(cfg);

    // Input / snap rotation
    g_fire_bool (ui, nullptr, "setMotherSnapRotation",           cfg.input.snap_rotation ? 1 : 0);
    g_fire_int  (ui, nullptr, "setMotherSnapRotationSegments",   cfg.input.snap_rotation_segments);
    g_fire_bool (ui, nullptr, "setMotherInputRelativeWalking",   cfg.input.vr_input_relative_walking ? 1 : 0);
    g_fire_bool (ui, nullptr, "setMotherInputStrafing",          cfg.input.vr_input_strafing ? 1 : 0);
    g_fire_bool (ui, nullptr, "setMotherToggleGrips",            cfg.input.toggle_grips ? 1 : 0);
    g_fire_bool (ui, nullptr, "setMotherToggleCrouch",           cfg.input.toggle_crouch ? 1 : 0);

    // The controls help on the options page: one row per "Action\tControl"
    // line of the layout in use.
    if (g_fire_str2) {
        for (const char* line : input::control_help()) {
            const std::string_view s(line);
            const auto tab = s.find('\t');
            const std::string action(s.substr(0, tab));
            const std::string control(tab == std::string_view::npos ? std::string_view{} : s.substr(tab + 1));
            g_fire_str2(ui, nullptr, "addMotherViewControl", action.c_str(), control.c_str());
        }
    }
}

// ---- on_menu_open -----------------------------------------------------------

void __fastcall on_menu_open(void* self, void* /*edx*/) noexcept {
    static bool s_logged = false;
    if (!std::exchange(s_logged, true)) {
        diag::logf("[script_bridge] menu opened (ui={}), pushing settings", self);
    }
    if (g_original_menu_open) g_original_menu_open(self, nullptr);
    push_settings_to_game(self);
}

// ---- handle_mvr_command -----------------------------------------------------
//
// Console command handler installed into AI.exe's command-dispatch slot at
// DAT_10066dc0 via WriteProcessMemory. Intercepts "mvr_*" tokens, applies
// them to config::current(), and chains to g_original_cmd for everything else.
// Corresponds to FUN_100202e0.
//
// Note: `cmd` is the raw console input with the first whitespace-terminated
// token as the command name. Only mvr_ commands are tokenised (in place);
// the original also tokenised the others before forwarding them, which cut
// their arguments off.

//
// One rule for bad values (spec 06 §4.2): a missing value, one that is not a
// number of its kind, one outside the accepted set, or an unknown mvr_ name
// changes nothing: no setting, no quit, no rebuild, no save. The original
// read a missing or malformed value as 0 (atoi), so a hand-typed or
// truncated command silently turned a switch off or quit the game.

void __fastcall handle_mvr_command(void* self, void* /*edx*/, int ctx, char* cmd, int p3, int p4) noexcept {
    // A null name is not ours to judge: the stock method gets the call
    // unchanged, as every call that is not an mvr_ command does.
    if (!cmd) {
        if (g_original_cmd) g_original_cmd(self, nullptr, ctx, cmd, p3, p4);
        return;
    }

    // Non-mvr_ commands go to the original handler untouched.
    const char* first = cmd;
    while (*first == ' ') ++first;
    if (std::strncmp(first, "mvr_", 4) != 0) {
        if (g_original_cmd) g_original_cmd(self, nullptr, ctx, cmd, p3, p4);
        return;
    }

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
                config::Config mapped{};
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
        reset_display_if_changed(cfg);

        // Persist the updated config to disk. Mirrors the FUN_1000d150() tail
        // call in FUN_100202e0 (an ofstream-based save wrapper). The original
        // also calls FUN_10003490() just before, but the result is unused,
        // likely an artefact of the compiler spilling ECX for a thiscall stub.
        // We omit it.
        config::save_current();

        // After the save: the new runtime is in the file before the game's
        // main loop reads the quit request.
        if (quit && g_exit_request) *g_exit_request = 1;
    } catch (...) {
        // Nothing may escape into the game's dispatcher.
        diag::log_line("[script_bridge] command failed: out of memory");
    }
}

} // namespace mothervr::integration
