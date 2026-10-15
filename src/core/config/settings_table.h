#pragma once

#include "core/config/settings.h"

#include <optional>
#include <string_view>

// The settings' value sets and ranges.
namespace omvr::core::config {

// Brings hand-edited or console-set values into range, in place: a pixel
// scale outside [0.5, 4] (or not a number) becomes 1.0, and the snap
// rotation segments are rounded down to one of 2, 3, 4, 6, 12, 24 (1 or
// less becomes 12). The runtime value Oculus becomes OpenXR (there is no
// LibOVR backend; Oculus means OpenXR). Run after every load and every
// mvr_* command.
// From: the guards in FUN_100202e0 (mvr_pixelscale) and FUN_10020870
// (the segment cascade).
void sanitize(Settings& cfg) noexcept;

// String forms used in the JSON. Parsing also takes the legacy spelling
// "Disabled" for VRRuntime::None.
std::string_view to_string(VRRuntime r) noexcept;
std::string_view to_string(MirrorViewType m) noexcept;
std::string_view to_string(StereoAA a) noexcept;
std::optional<VRRuntime>      parse_vr_runtime(std::string_view s) noexcept;
std::optional<MirrorViewType> parse_mirror_view_type(std::string_view s) noexcept;
std::optional<StereoAA>       parse_stereo_aa(std::string_view s) noexcept;

} // namespace omvr::core::config
