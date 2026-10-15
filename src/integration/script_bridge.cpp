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
#include "diag/log.h"
#include "input/vr_controls.h"
#include "runtime/vr_detect.h"
#include "stereo/game_hooks.h"
#include "stereo/stereo_device.h"

#include <cstring>  // strtok_s, strncmp, strcmp
#include <cstdlib>  // atoi, atof
#include <string>
#include <string_view>
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
    built = now;
    if (g_display_reset_flag) *g_display_reset_flag = 1;
}

} // namespace

// ---- push_settings_to_game --------------------------------------------------
//
// Pushes the entire live config into the game's script-variable layer, on the
// menu object `ui`. Corresponds to FUN_10020870.

void push_settings_to_game(void* ui) noexcept {
    if (!ui || !callbacks_ready()) {
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

void __fastcall handle_mvr_command(void* self, void* /*edx*/, int ctx, char* cmd, int p3, int p4) noexcept {
    if (!cmd) return;

    // Non-mvr_ commands go to the original handler untouched.
    const char* first = cmd;
    while (*first == ' ') ++first;
    if (std::strncmp(first, "mvr_", 4) != 0) {
        if (g_original_cmd) g_original_cmd(self, nullptr, ctx, cmd, p3, p4);
        return;
    }

    // First token = command name (strtok_s mutates cmd in place).
    char* rest = nullptr;
    const char* name = strtok_s(cmd, " ", &rest);
    if (!name) return;

    auto& cfg = config::current();

    // Each branch: strtok_s(nullptr, ...) gets the value token; atoi/atof
    // converts it. Mirrors the strtok-pair pattern throughout FUN_100202e0.

    if (std::strcmp(name, "mvr_vrruntime") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        const int n = val ? std::atoi(val) : 0;
        if (n >= 0 && n <= 2) {
            cfg.vr_runtime = runtime_from_ui(n);
            // The new runtime takes effect on the next launch, so the game
            // quits, as in the original.
            if (g_exit_request) *g_exit_request = 1;
        }

    } else if (std::strcmp(name, "mvr_debug") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.debug.debug = val && std::atoi(val) != 0;

    } else if (std::strcmp(name, "mvr_skipintro") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.debug.skip_startup_intro = val && std::atoi(val) != 0;

    } else if (std::strcmp(name, "mvr_mirrorview") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        // 0 both eyes, 1 right eye, 2 left eye; anything else is ignored.
        const int n = val ? std::atoi(val) : -1;
        if (n >= 0 && n <= static_cast<int>(config::MirrorViewType::LeftEye)) {
            cfg.rendering.mirror_view_type = static_cast<config::MirrorViewType>(n);
        }

    } else if (std::strcmp(name, "mvr_mirrorviewsym") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.rendering.symmetric_mirror_view = val && std::atoi(val) != 0;

    } else if (std::strcmp(name, "mvr_pixelscale") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        // Out of [0.5, 4.0] resets to 1.0 (config::sanitize below), the
        // guard at FUN_100202e0 lines 183-185.
        cfg.rendering.pixel_scale = val ? static_cast<float>(std::atof(val)) : 1.0f;

    } else if (std::strcmp(name, "mvr_clipblinding") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.rendering.clipping_blinders = val && std::atoi(val) != 0;
        // Takes effect at once: the game reads the threshold through a
        // pointer patched into its head-in-wall check.
        stereo::set_clipping_blinders(cfg.rendering.clipping_blinders);

    } else if (std::strcmp(name, "mvr_snaprotation") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.input.snap_rotation = val && std::atoi(val) != 0;

    } else if (std::strcmp(name, "mvr_snaprotationsegments") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.input.snap_rotation_segments = val ? std::atoi(val) : 12;

    } else if (std::strcmp(name, "mvr_inputrelativewalking") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.input.vr_input_relative_walking = val && std::atoi(val) != 0;

    } else if (std::strcmp(name, "mvr_inputstrafing") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.input.vr_input_strafing = val && std::atoi(val) != 0;

    } else if (std::strcmp(name, "mvr_togglegrips") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.input.toggle_grips = val && std::atoi(val) != 0;

    } else if (std::strcmp(name, "mvr_togglecrouch") == 0) {
        const char* val = strtok_s(nullptr, " ", &rest);
        cfg.input.toggle_crouch = val && std::atoi(val) != 0;
    }

    config::sanitize(cfg);
    // Pixel scale and mirror changes need the game's targets rebuilt.
    reset_display_if_changed(cfg);

    // Persist the updated config to disk. Mirrors the FUN_1000d150() tail call
    // in FUN_100202e0 (an ofstream-based save wrapper). The original also calls
    // FUN_10003490() just before, but the result is unused, likely an artefact
    // of the compiler spilling ECX for a thiscall stub. We omit it.
    config::save_current();
}

} // namespace mothervr::integration
