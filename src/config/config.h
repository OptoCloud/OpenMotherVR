#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace mothervr::config {

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
inline constexpr float kStereoAaMaxAutoScale = 1.3f;

// Defaults are the original's, except the runtime (see Config).
struct RenderingSettings {
    MirrorViewType mirror_view_type     = MirrorViewType::RightEye;
    bool           symmetric_mirror_view = true;
    float          pixel_scale           = 1.0f;
    bool           clipping_blinders     = true;
    StereoAA       stereo_aa             = StereoAA::Auto;
};

// Whether the game's anti-aliasing should run in stereo with `r`.
inline bool stereo_aa_enabled(const RenderingSettings& r) noexcept {
    return r.stereo_aa == StereoAA::On
        || (r.stereo_aa == StereoAA::Auto && r.pixel_scale <= kStereoAaMaxAutoScale);
}

struct InputSettings {
    bool snap_rotation             = true;
    int  snap_rotation_segments    = 12;
    bool vr_input_relative_walking = true;
    bool vr_input_strafing         = true;
    bool toggle_grips              = false;
    bool toggle_crouch             = false;
};

struct DebugSettings {
    bool skip_startup_intro = false;
    // "Dev. Debug Options" in the MotherVR menu: unhides Select Level and the
    // debug checkpoints. Stored as Settings.DebugOptions.
    bool debug              = false;
};

struct Config {
    // The original defaults to None. OpenVR is the runtime we have a stereo
    // device for, so a first launch starts in VR.
    VRRuntime         vr_runtime = VRRuntime::OpenVR;
    RenderingSettings rendering{};
    InputSettings     input{};
    DebugSettings     debug{};
};

// Load mothervr.json from `path` (typically the game's working dir).
// On any I/O or parse error returns std::nullopt; the caller is expected to
// fall back to defaults. Missing keys and values of the wrong type take
// their defaults one by one.
//
// From: FUN_10010750 @ 0x10010750 (the file-open + parse wrapper); the deep
// parse machinery lives in nlohmann::json itself.
std::optional<Config> load(const std::filesystem::path& path);

// Write `cfg` to `path` as pretty-printed JSON (4-space indent, as
// nlohmann's `dump(4)`) in the original's layout. Keys of the file last
// loaded that we do not know are kept.
//
// From: FUN_10014680 @ 0x10014680.
void save(const std::filesystem::path& path, const Config& cfg);

// Brings hand-edited or console-set values into range, in place: a pixel
// scale outside [0.5, 4] (or not a number) becomes 1.0, and the snap
// rotation segments are rounded down to one of 2, 3, 4, 6, 12, 24 (1 or
// less becomes 12). Run after every load and every mvr_* command.
// From: the guards in FUN_100202e0 (mvr_pixelscale) and FUN_10020870
// (the segment cascade).
void sanitize(Config& cfg) noexcept;

// Process-wide config singleton. Reads/writes are not thread-safe: the
// original calls into config from DllMain and from the game's UI thread
// only, both of which run on the main thread once initialized.
Config& current() noexcept;

// One-time initialization: tries to load mothervr.json from `dir`. Without
// a file, populates `current()` with defaults and returns true; the caller
// then writes the template with save_current(), as the original always has
// a mothervr.json on disk after the first launch. (DllMain does that once
// the welcome box is closed, so a process that exits before showing it,
// such as a Steam relaunch, leaves the first launch to the next one.) A
// file that does not parse also gives defaults, but is left alone until a
// setting changes.
// Safe to call multiple times: re-loading is idempotent.
//
// `dir` is typically the directory containing our DLL (= the AI install dir);
// passing the explicit path avoids relying on the process CWD which can be
// surprising under Steam's launcher.
// Returns true when there was no mothervr.json, i.e. on the first launch.
bool initialize(const std::filesystem::path& dir);

// Persist the current config to the directory passed to initialize().
// No-op if initialize() has not been called yet. Called from the script
// bridge after any in-game mvr_* console command changes a setting.
void save_current();

// String forms used in the JSON. Parsing also takes the legacy spelling
// "Disabled" for VRRuntime::None.
std::string_view to_string(VRRuntime r) noexcept;
std::string_view to_string(MirrorViewType m) noexcept;
std::string_view to_string(StereoAA a) noexcept;
std::optional<VRRuntime>      parse_vr_runtime(std::string_view s) noexcept;
std::optional<MirrorViewType> parse_mirror_view_type(std::string_view s) noexcept;
std::optional<StereoAA>       parse_stereo_aa(std::string_view s) noexcept;

} // namespace mothervr::config
