#pragma once

#include "core/config/settings.h"

#include <optional>
#include <span>
#include <string_view>
#include <variant>

// The settings of mothervr.json, one row each: where the value lives in the
// file, where in Settings, and the console command that sets it. Reading,
// writing, describing and the mvr_* commands all go through this table, so
// a setting is named in one place.
namespace omvr::core::config {

// The JSON object a key is in: "Settings" itself, or one of its objects.
// HandsHaptics is "Haptics" inside "Hands".
enum class Section { Settings, Rendering, Input, Comfort, Controls, Hands, HandsHaptics, Body };

// A setting's place in Settings.
template <class T>
using Accessor = T& (*)(Settings&) noexcept;

using Field = std::variant<Accessor<bool>, Accessor<int>, Accessor<float>, Accessor<VRRuntime>,
                           Accessor<MirrorViewType>, Accessor<StereoAA>, Accessor<PlayMode>,
                           Accessor<FlashlightAim>, Accessor<BodyVisible>>;

// How the setting's console command reads its value (spec 06 §4.2): a value
// outside the rule changes nothing.
enum class CommandRule {
    None,         // no command
    Switch,       // 0 or 1
    Choice,       // an enum's integer, 0 to its last value
    PixelScale,   // a number in [kPixelScaleMin, kPixelScaleMax]
    Segments,     // an integer of at least kSnapSegmentsMinCommand, rounded down to a step
    RuntimeEntry, // the options page's runtime entry (runtime_menu.h)
    Number,       // a decimal number in the row's range
    Integer,      // a decimal integer in the row's range
};

// The values a Number or Integer command accepts: [min, max], and 0 too
// where 0 means off (spec 22 §3.1).
struct CommandRange {
    float min    = 0.0f;
    float max    = 0.0f;
    bool or_zero = false;

    constexpr bool accepts(float v) const noexcept {
        return (v >= min && v <= max) || (or_zero && v == 0.0f);
    }
};

// What else an accepted command does.
enum class CommandEffect {
    None,
    QuitGame,        // the runtime takes effect on the next launch, so the game quits
    ApplyBlinders,   // the game reads the head-clip threshold at once
    CalibrateHeight, // no setting: measure the standing eye height (spec 22 §3.3, §6.2)
};

struct SettingRow {
    Section section;
    std::string_view key;  // in its section
    std::string_view path; // the key path in the log ("Rendering.PixelScale")
    Field field;
    std::string_view command = {}; // "mvr_..." or empty
    CommandRule rule         = CommandRule::None;
    CommandEffect effect     = CommandEffect::None;
    CommandRange range       = {}; // for CommandRule::Number and Integer
    // The options page's function the value is pushed to (spec 06 §4.3), for
    // the rows the push sends generically (the v2 pages); empty for the v1
    // rows, which the push names one by one.
    std::string_view push = {};
};

// Every setting, in the order the original's file lists them.
std::span<const SettingRow> settings_table() noexcept;

// The row of the console command `name`, or null.
const SettingRow* find_command(std::string_view name) noexcept;

// Brings hand-edited or console-set values into range, in place: a pixel
// scale outside [kPixelScaleMin, kPixelScaleMax] (or not a number) becomes
// kPixelScaleDefault, and the snap rotation segments are rounded down to
// one of kSnapSegmentSteps (below the smallest: kSnapSegmentsDefault), and
// the comfort values are held to their ranges (spec 22 §3.1). The
// runtime value Oculus becomes OpenXR (there is no LibOVR backend; Oculus
// means OpenXR). Run after every load and every mvr_* command.
// From: the guards in FUN_100202e0 (mvr_pixelscale) and FUN_10020870
// (the segment cascade).
void sanitize(Settings& cfg) noexcept;

// `segments` rounded down to one of kSnapSegmentSteps, or the default.
int snap_segments(int segments) noexcept;

// String forms used in the JSON. Parsing also takes the legacy spelling
// "Disabled" for VRRuntime::None.
std::string_view to_string(VRRuntime r) noexcept;
std::string_view to_string(MirrorViewType m) noexcept;
std::string_view to_string(StereoAA a) noexcept;
std::string_view to_string(PlayMode m) noexcept;
std::string_view to_string(FlashlightAim a) noexcept;
std::string_view to_string(BodyVisible v) noexcept;
std::optional<VRRuntime> parse_vr_runtime(std::string_view s) noexcept;
std::optional<MirrorViewType> parse_mirror_view_type(std::string_view s) noexcept;
std::optional<StereoAA> parse_stereo_aa(std::string_view s) noexcept;
std::optional<PlayMode> parse_play_mode(std::string_view s) noexcept;
std::optional<FlashlightAim> parse_flashlight_aim(std::string_view s) noexcept;
std::optional<BodyVisible> parse_body_visible(std::string_view s) noexcept;

// The string parser and the last value of each enum the file stores.
template <class E>
struct EnumTraits;
template <>
struct EnumTraits<VRRuntime> {
    static constexpr VRRuntime kLast = VRRuntime::OpenXR;
    static std::optional<VRRuntime> parse(std::string_view s) noexcept { return parse_vr_runtime(s); }
};
template <>
struct EnumTraits<MirrorViewType> {
    static constexpr MirrorViewType kLast = MirrorViewType::LeftEye;
    static std::optional<MirrorViewType> parse(std::string_view s) noexcept {
        return parse_mirror_view_type(s);
    }
};
template <>
struct EnumTraits<StereoAA> {
    static constexpr StereoAA kLast = StereoAA::Off;
    static std::optional<StereoAA> parse(std::string_view s) noexcept { return parse_stereo_aa(s); }
};

template <>
struct EnumTraits<PlayMode> {
    static constexpr PlayMode kLast = PlayMode::Standing;
    static std::optional<PlayMode> parse(std::string_view s) noexcept { return parse_play_mode(s); }
};
template <>
struct EnumTraits<FlashlightAim> {
    static constexpr FlashlightAim kLast = FlashlightAim::RightHand;
    static std::optional<FlashlightAim> parse(std::string_view s) noexcept { return parse_flashlight_aim(s); }
};
template <>
struct EnumTraits<BodyVisible> {
    static constexpr BodyVisible kLast = BodyVisible::None;
    static std::optional<BodyVisible> parse(std::string_view s) noexcept { return parse_body_visible(s); }
};

} // namespace omvr::core::config
