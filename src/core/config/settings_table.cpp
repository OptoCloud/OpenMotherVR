#include "core/config/settings_table.h"

#include <algorithm>
#include <array>

namespace omvr::core::config {

namespace {

// Where each setting lives in Settings.
VRRuntime&      vr_runtime(Settings& s) noexcept { return s.vr_runtime; }
MirrorViewType& mirror_view_type(Settings& s) noexcept { return s.rendering.mirror_view_type; }
bool&           symmetric_mirror_view(Settings& s) noexcept { return s.rendering.symmetric_mirror_view; }
float&          pixel_scale(Settings& s) noexcept { return s.rendering.pixel_scale; }
bool&           clipping_blinders(Settings& s) noexcept { return s.rendering.clipping_blinders; }
StereoAA&       stereo_aa(Settings& s) noexcept { return s.rendering.stereo_aa; }
bool&           snap_rotation(Settings& s) noexcept { return s.input.snap_rotation; }
int&            snap_rotation_segments(Settings& s) noexcept { return s.input.snap_rotation_segments; }
bool&           relative_walking(Settings& s) noexcept { return s.input.vr_input_relative_walking; }
bool&           strafing(Settings& s) noexcept { return s.input.vr_input_strafing; }
bool&           toggle_grips(Settings& s) noexcept { return s.input.toggle_grips; }
bool&           toggle_crouch(Settings& s) noexcept { return s.input.toggle_crouch; }
bool&           debug_options(Settings& s) noexcept { return s.debug.debug; }
bool&           skip_startup_intro(Settings& s) noexcept { return s.debug.skip_startup_intro; }

// Key names are the original's (spec 06 §3); "StereoAntiAliasing" and the
// OpenXR runtime value are this project's.
constexpr std::array kRows{
    SettingRow{Section::Settings, "VRRuntime", "VRRuntime", &vr_runtime, "mvr_vrruntime",
               CommandRule::RuntimeEntry, CommandEffect::QuitGame},
    SettingRow{Section::Rendering, "MirrorViewType", "Rendering.MirrorViewType", &mirror_view_type,
               "mvr_mirrorview", CommandRule::Choice},
    SettingRow{Section::Rendering, "SymmetricMirrorView", "Rendering.SymmetricMirrorView", &symmetric_mirror_view,
               "mvr_mirrorviewsym", CommandRule::Switch},
    SettingRow{Section::Rendering, "PixelScale", "Rendering.PixelScale", &pixel_scale, "mvr_pixelscale",
               CommandRule::PixelScale},
    SettingRow{Section::Rendering, "ClippingBlinders", "Rendering.ClippingBlinders", &clipping_blinders,
               "mvr_clipblinding", CommandRule::Switch, CommandEffect::ApplyBlinders},
    SettingRow{Section::Rendering, "StereoAntiAliasing", "Rendering.StereoAntiAliasing", &stereo_aa},
    SettingRow{Section::Input, "SnapRotation", "Input.SnapRotation", &snap_rotation, "mvr_snaprotation",
               CommandRule::Switch},
    SettingRow{Section::Input, "SnapRotationSegments", "Input.SnapRotationSegments", &snap_rotation_segments,
               "mvr_snaprotationsegments", CommandRule::Segments},
    SettingRow{Section::Input, "VRInputRelativeWalking", "Input.VRInputRelativeWalking", &relative_walking,
               "mvr_inputrelativewalking", CommandRule::Switch},
    SettingRow{Section::Input, "VRInputStrafing", "Input.VRInputStrafing", &strafing, "mvr_inputstrafing",
               CommandRule::Switch},
    SettingRow{Section::Input, "ToggleGrips", "Input.ToggleGrips", &toggle_grips, "mvr_togglegrips",
               CommandRule::Switch},
    SettingRow{Section::Input, "ToggleCrouch", "Input.ToggleCrouch", &toggle_crouch, "mvr_togglecrouch",
               CommandRule::Switch},
    // "Dev. Debug Options" on the options page.
    SettingRow{Section::Settings, "DebugOptions", "DebugOptions", &debug_options, "mvr_debug", CommandRule::Switch},
    SettingRow{Section::Settings, "SkipStartupIntro", "SkipStartupIntro", &skip_startup_intro, "mvr_skipintro",
               CommandRule::Switch},
};

} // namespace

std::span<const SettingRow> settings_table() noexcept { return kRows; }

const SettingRow* find_command(std::string_view name) noexcept {
    const auto it =
        std::ranges::find_if(kRows, [name](const SettingRow& r) { return !r.command.empty() && r.command == name; });
    return it == kRows.end() ? nullptr : &*it;
}

std::string_view to_string(VRRuntime r) noexcept {
    switch (r) {
        case VRRuntime::None:   return "None";
        case VRRuntime::Oculus: return "Oculus";
        case VRRuntime::OpenVR: return "OpenVR";
        case VRRuntime::OpenXR: return "OpenXR";
    }
    return "OpenVR";
}

std::string_view to_string(MirrorViewType m) noexcept {
    switch (m) {
        case MirrorViewType::BothEyes: return "BothEyes";
        case MirrorViewType::RightEye: return "RightEye";
        case MirrorViewType::LeftEye:  return "LeftEye";
    }
    return "RightEye";
}

std::string_view to_string(StereoAA a) noexcept {
    switch (a) {
        case StereoAA::Auto: return "Auto";
        case StereoAA::On:   return "On";
        case StereoAA::Off:  return "Off";
    }
    return "Auto";
}

std::optional<StereoAA> parse_stereo_aa(std::string_view s) noexcept {
    if (s == "Auto") return StereoAA::Auto;
    if (s == "On")   return StereoAA::On;
    if (s == "Off")  return StereoAA::Off;
    return std::nullopt;
}

std::optional<VRRuntime> parse_vr_runtime(std::string_view s) noexcept {
    if (s == "None" || s == "Disabled") return VRRuntime::None;
    if (s == "Oculus") return VRRuntime::Oculus;
    if (s == "OpenVR") return VRRuntime::OpenVR;
    if (s == "OpenXR") return VRRuntime::OpenXR;
    return std::nullopt;
}

std::optional<MirrorViewType> parse_mirror_view_type(std::string_view s) noexcept {
    if (s == "BothEyes") return MirrorViewType::BothEyes;
    if (s == "RightEye") return MirrorViewType::RightEye;
    if (s == "LeftEye")  return MirrorViewType::LeftEye;
    return std::nullopt;
}

int snap_segments(int segments) noexcept {
    for (const int step : kSnapSegmentSteps) {
        if (segments >= step) return step;
    }
    return kSnapSegmentsDefault;
}

void sanitize(Settings& cfg) noexcept {
    // Oculus means OpenXR (spec 06 §1.1): Meta's PC runtime is an OpenXR
    // runtime and there is no LibOVR backend, so the runtime choice and the
    // next write see OpenXR.
    if (cfg.vr_runtime == VRRuntime::Oculus) cfg.vr_runtime = VRRuntime::OpenXR;

    // Written so that NaN, which compares false, is out of range too.
    float& scale = cfg.rendering.pixel_scale;
    if (!(scale >= kPixelScaleMin && scale <= kPixelScaleMax)) scale = kPixelScaleDefault;

    cfg.input.snap_rotation_segments = snap_segments(cfg.input.snap_rotation_segments);
}

} // namespace omvr::core::config
