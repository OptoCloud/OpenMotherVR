#include "core/config/settings_table.h"

namespace omvr::core::config {

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


void sanitize(Settings& cfg) noexcept {
    // Oculus means OpenXR (spec 06 §1.1): Meta's PC runtime is an OpenXR
    // runtime and there is no LibOVR backend, so the runtime choice and the
    // next write see OpenXR.
    if (cfg.vr_runtime == VRRuntime::Oculus) cfg.vr_runtime = VRRuntime::OpenXR;

    float& scale = cfg.rendering.pixel_scale;
    if (!(scale >= 0.5f && scale <= 4.0f)) scale = 1.0f;

    int& segments = cfg.input.snap_rotation_segments;
    segments = segments >= 24 ? 24
             : segments >= 12 ? 12
             : segments >= 6  ? 6
             : segments >= 4  ? 4
             : segments >= 3  ? 3
             : segments >= 2  ? 2
                              : 12;  // 1 or less: the default, 30 degrees a step
}

} // namespace omvr::core::config
