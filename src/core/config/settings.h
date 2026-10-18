#pragma once

#include "core/config/tuning.h"

namespace omvr::core::config {

// JSON schema recovered from .rdata strings at 0x1005C9C8..0x1005CAC4.
// Key names are verbatim from the original's strings. mothervr.json as
// MotherVR 0.8.1 writes it, and as we write it:
//
//   {
//     "Settings": {
//       "VRRuntime": "None" | "Oculus" | "OpenVR" | "OpenXR",   (OpenXR is ours)
//       "Rendering": {
//         "MirrorViewType":      "BothEyes" | "RightEye" | "LeftEye",
//         "SymmetricMirrorView": bool,
//         "PixelScale":          number,
//         "ClippingBlinders":    bool,
//         "StereoAntiAliasing":  "Auto" | "On" | "Off"     (ours)
//       },
//       "Input": {
//         "SnapRotation":          bool,
//         "SnapRotationSegments":  number,
//         "VRInputRelativeWalking":bool,
//         "VRInputStrafing":       bool,
//         "ToggleGrips":           bool,
//         "ToggleCrouch":          bool
//       },
//       "Comfort":  { ... },   (ours, v2: spec 22 §3.1)
//       "Controls": { ... },   (ours, v2: spec 22 §3.1)
//       "Hands":    { ..., "Haptics": { ... } },   (ours, v2: spec 21 §15.1)
//       "Body":     { ... },   (ours, v2: spec 21 §15.2)
//       "Graphics": { ... },   (ours, v2: spec 24 §10)
//       "DebugOptions":     bool,   (the "Dev. Debug Options" flag)
//       "SkipStartupIntro": bool
//     }
//   }
//
// Also read: the layout earlier builds of this reconstruction wrote, with a
// top-level "DebugOptions" object holding "SkipStartupIntro" and
// "DevDebugOptions", and "Disabled" for the runtime None; and integers where
// an enum string is expected. Keys we do not know are kept when the file is
// rewritten. Spec: openmothervr/spec/06-options-and-config.md section 3.
//
// The serialization library is nlohmann::json 3.4.0 (the version fingerprint
// `961c151d2e87f2686a955a9be24d316f1362bf21 3.4.0` is in .rdata).

// The original's integers, pushed to the game UI via setMotherSetVRRuntime
// and read back from mvr_vrruntime (the menu's "Disable VR" / "Use Oculus" /
// "Use SteamVR" items send 0 / 1 / 2). OpenXR is ours and is appended so
// those stay unchanged.
enum class VRRuntime { None = 0, Oculus = 1, OpenVR = 2, OpenXR = 3 };

// The original's integers, used by mvr_mirrorview, setMotherMirrorView and
// the mirror shader constants.
enum class MirrorViewType { BothEyes = 0, RightEye = 1, LeftEye = 2 };

// The game's anti-aliasing in stereo (ours; the original had none there).
// Auto runs it only up to kStereoAaMaxAutoScale: above that the eyes are
// supersampled enough that it costs about 0.6 ms GPU for little gain.
enum class StereoAA { Auto = 0, On = 1, Off = 2 };

// v2 (ours). Each default is v1's behaviour (spec 20 §1.1).
enum class PlayMode { Seated = 0, Standing = 1 };
enum class FlashlightAim { Head = 0, LeftHand = 1, RightHand = 2 };
enum class BodyVisible { Full = 0, ArmsAndHands = 1, HandsOnly = 2, None = 3 };

// Defaults are the original's, except the runtime (see Config).
struct RenderingSettings {
    MirrorViewType mirror_view_type = MirrorViewType::RightEye;
    bool symmetric_mirror_view      = true;
    float pixel_scale               = kPixelScaleDefault;
    bool clipping_blinders          = true;
    StereoAA stereo_aa              = StereoAA::Auto;
};

// Whether the game's anti-aliasing should run in stereo with `r`.
inline bool stereo_aa_enabled(const RenderingSettings& r) noexcept {
    return r.stereo_aa == StereoAA::On ||
           (r.stereo_aa == StereoAA::Auto && r.pixel_scale <= kStereoAaMaxAutoScale);
}

struct InputSettings {
    bool snap_rotation             = true;
    int snap_rotation_segments     = kSnapSegmentsDefault;
    bool vr_input_relative_walking = true;
    bool vr_input_strafing         = true;
    bool toggle_grips              = false;
    bool toggle_crouch             = false;
};

// v2 comfort (spec 22 §3.1). Each default is v1's behaviour.
struct ComfortSettings {
    // Degrees per second; 0 turns through the game's own stick look.
    float smooth_turn_speed = 0.0f;
    // A held stick repeats a snap step this often; 0 never.
    int snap_turn_repeat_ms = kSnapTurnRepeatMsDefault;
    // A short fade from black after each snap step.
    bool snap_turn_fade = false;
    // Comfort vignette strength while moving or turning, 0 off to 1.
    float vignette     = 0.0f;
    PlayMode play_mode = PlayMode::Seated;
    // Metres; 0 is not calibrated.
    float standing_eye_height = 0.0f;
    float world_scale         = kWorldScaleDefault;
};

// v2 controls (spec 22 §3.1); every one off is v1.
struct ControlsSettings {
    bool interaction_aware_turning = false;
    bool dpad_emulation            = false;
    bool recenter_gesture          = false;
    bool hud_toggle_gesture        = false;
    bool holster_gestures          = false;
    bool hiding_lean               = false;
    bool hold_breath_gesture       = false;
    bool controller_glyphs         = false;
    bool vr_off_hotkey             = false;
    // Read at attach: applies from the next launch.
    bool crash_safe_log = false;
};

// Spec 21 §15.1. The sub-keys default on so that turning the master key
// on gives the whole feature; the master keys default off.
struct HapticsSettings {
    bool enabled     = false; // read at runtime start
    float strength   = kHapticStrengthMax;
    bool game_rumble = true;
    bool damage      = true;
    bool tracker     = true;
    bool firing      = true;
    bool hacking     = true;
};

struct HandsSettings {
    bool enabled                 = false;
    bool aim_from_hand           = true;
    bool tracker_in_hand         = true;
    FlashlightAim flashlight_aim = FlashlightAim::Head;
    bool melee_by_motion         = true;
    bool throw_by_motion         = true;
    float throw_strength         = kThrowStrengthDefault;
    float smoothing              = 0.0f;
    bool wrist_health            = false;
    HapticsSettings haptics{};
};

// Spec 21 §15.2.
struct BodySettings {
    BodyVisible visible    = BodyVisible::Full;
    float eye_forward      = 0.0f; // metres
    bool room_scale        = false;
    float seated_eye_raise = 0.0f; // metres
    bool real_crouch       = false;
};

// v2 rendering (spec 24 §10). Each default is v1's behaviour. Read at
// attach: the engine settings override and its additions apply from the
// next launch (24 §6.1).
struct GraphicsSettings {
    bool engine_settings_override = false;
    bool shadow_map_4096          = false;
    bool sharp_reflections        = false;
    // Megabytes; 0 keeps the game's own size.
    int streaming_texture_heap_mb = 0;
    int streaming_model_heap_mb   = 0;
};

struct DebugSettings {
    bool skip_startup_intro = false;
    // "Dev. Debug Options" in the MotherVR menu: unhides Select Level and the
    // debug checkpoints. Stored as Settings.DebugOptions.
    bool debug = false;
};

struct Settings {
    // The original defaults to None. OpenVR is the runtime we have a stereo
    // device for, so a first launch starts in VR.
    VRRuntime vr_runtime = VRRuntime::OpenVR;
    RenderingSettings rendering{};
    InputSettings input{};
    DebugSettings debug{};
    // Ours, v2: each default is v1's behaviour.
    ComfortSettings comfort{};
    ControlsSettings controls{};
    HandsSettings hands{};
    BodySettings body{};
    GraphicsSettings graphics{};
    // Ours: show the game and the VR runtime on the player's Discord
    // profile. Stored as Settings.DiscordRichPresence.
    bool discord_rich_presence = true;
};

} // namespace omvr::core::config
