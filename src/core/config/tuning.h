#pragma once

#include <array>
#include <cstddef>

namespace omvr::core::config {

// The settings file, in the directory of the DLL (spec 06 §3).
inline constexpr const wchar_t* kSettingsFileName = L"mothervr.json";

// A file replaced in one step (mothervr.json, MOTHER.PAK) is first written
// whole under its name with this suffix, then moved over the old one (spec
// 06 §3, 14 §5.8). The platform's writer and the log lines both use it.
inline constexpr const wchar_t* kTemporaryFileSuffix = L".tmp";

// mothervr.json is written pretty-printed with this indent, as the original
// writes it (nlohmann's dump(4)), so a player's diff stays readable.
inline constexpr int kSettingsJsonIndent = 4;

// Notepad and other Windows editors may save a UTF-8 byte-order mark;
// players edit the file by hand, so it is skipped on reading.
inline constexpr std::array<unsigned char, 3> kUtf8ByteOrderMark{0xEF, 0xBB, 0xBF};

// Rendering.PixelScale: the accepted range, and the value an out-of-range
// one becomes. From the guard in FUN_100202e0 (mvr_pixelscale).
inline constexpr float kPixelScaleMin     = 0.5f;
inline constexpr float kPixelScaleMax     = 4.0f;
inline constexpr float kPixelScaleDefault = 1.0f;

// Input.SnapRotationSegments: the step counts a turn is split into, largest
// first; any other value is rounded down to one of them, and a value below
// the smallest becomes the default (12: 30 degrees a step). From the
// segment cascade in FUN_10020870.
inline constexpr std::array<int, 6> kSnapSegmentSteps{24, 12, 6, 4, 3, 2};
inline constexpr int kSnapSegmentsDefault = 12;
// mvr_snaprotationsegments refuses fewer than this (spec 06 §4.2).
inline constexpr int kSnapSegmentsMinCommand = 2;

// -vrscale <s>: the world scale accepted, and the scale without a usable
// value (spec 03 §1, 06 §2).
inline constexpr float kWorldScaleMin     = 0.495f;
inline constexpr float kWorldScaleMax     = 1.505f;
inline constexpr float kWorldScaleDefault = 1.0f;

// Stereo anti-aliasing "Auto" runs only up to this pixel scale: above it the
// eyes are supersampled enough that it costs about 0.6 ms GPU for little gain.
inline constexpr float kStereoAaMaxAutoScale = 1.3f;

} // namespace omvr::core::config
