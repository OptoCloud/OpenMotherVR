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
    // Ours: show the game and the VR runtime on the player's Discord
    // profile. Stored as Settings.DiscordRichPresence.
    bool discord_rich_presence = true;
};

} // namespace omvr::core::config
