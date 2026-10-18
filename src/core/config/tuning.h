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

// Comfort.SmoothTurnSpeed, degrees per second (spec 22 §3.1): 0 keeps the
// game's own stick look; any other value is held to [min, max]. Below 30 a
// full circle takes over 12 s; above 360 it is no smoother than a snap.
inline constexpr float kSmoothTurnSpeedMin = 30.0f;
inline constexpr float kSmoothTurnSpeedMax = 360.0f;

// Comfort.SnapTurnRepeatMs (spec 22 §3.1): 0 steps once per deflection;
// any other value is held to [min, max]; a negative one is the default,
// v1's fixed 300 ms.
inline constexpr int kSnapTurnRepeatMsMin     = 150;
inline constexpr int kSnapTurnRepeatMsMax     = 1000;
inline constexpr int kSnapTurnRepeatMsDefault = 300;

// Comfort.Vignette (spec 22 §3.1): 0 off to 1 strongest.
inline constexpr float kVignetteMax = 1.0f;

// Comfort.StandingEyeHeight, metres (spec 22 §3.1): 0 is not calibrated;
// a calibrated height lies in [min, max], anything else becomes 0.
inline constexpr float kStandingEyeHeightMin = 1.0f;
inline constexpr float kStandingEyeHeightMax = 2.2f;

// Hands (spec 21 §15.1).
inline constexpr float kThrowStrengthMin     = 0.5f;
inline constexpr float kThrowStrengthMax     = 2.0f;
inline constexpr float kThrowStrengthDefault = 1.0f;
inline constexpr float kHandSmoothingMax     = 1.0f;
inline constexpr float kHapticStrengthMax    = 1.0f;

// Body (spec 21 §15.2), metres; a value outside is clamped.
inline constexpr float kEyeForwardMax     = 0.20f;
inline constexpr float kSeatedEyeRaiseMax = 0.50f;

// Graphics.StreamingTextureHeapMB and StreamingModelHeapMB, megabytes (spec
// 24 §6.2, §10): 0 keeps the game's size; any other value is held to [min,
// max], a negative one is 0. The minimums are the stock sizes; the maximums
// keep a 32-bit process's address space safe.
inline constexpr int kTextureHeapMbMin = 300;
inline constexpr int kTextureHeapMbMax = 1024;
inline constexpr int kModelHeapMbMin   = 128;
inline constexpr int kModelHeapMbMax   = 512;

// -vrscale <s>: the world scale accepted, and the scale without a usable
// value (spec 03 §1, 06 §2).
inline constexpr float kWorldScaleMin     = 0.495f;
inline constexpr float kWorldScaleMax     = 1.505f;
inline constexpr float kWorldScaleDefault = 1.0f;

// Stereo anti-aliasing "Auto" runs only up to this pixel scale: above it the
// eyes are supersampled enough that it costs about 0.6 ms GPU for little gain.
inline constexpr float kStereoAaMaxAutoScale = 1.3f;

} // namespace omvr::core::config
