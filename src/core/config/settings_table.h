#pragma once

#include "core/config/settings.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

// The settings of mothervr.json, one row each: where the value lives in the
// file, where in Settings, and the console command that sets it. Reading,
// writing, describing and the mvr_* commands all go through this table, so
// a setting is named in one place.
namespace omvr::core::config {

// The JSON object a key is in: "Settings" itself, or one of its objects.
// HandsHaptics is "Haptics" inside "Hands".
enum class Section {
    Settings,
    Rendering,
    Input,
    Comfort,
    Controls,
    Hands,
    HandsHaptics,
    Body,
    Graphics,
    Interaction,
    Hud,
};

// A setting's place in Settings.
template <class T>
using Accessor = T& (*)(Settings&) noexcept;

// The accessor of the member reached through `Members`, outermost first:
// &at<&Settings::rendering, &RenderingSettings::pixel_scale>.
template <auto... Members>
constexpr auto& at(Settings& s) noexcept {
    return (s.*....*Members);
}

using Field = std::variant<Accessor<bool>, Accessor<int>, Accessor<float>, Accessor<VRRuntime>,
                           Accessor<MirrorViewType>, Accessor<StereoAA>, Accessor<PlayMode>,
                           Accessor<FlashlightAim>, Accessor<BodyVisible>, Accessor<CameraTakeovers>,
                           Accessor<CrosshairMode>, Accessor<HudPlacement>>;

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
    ApplyPostFx,     // the 2D effect hooks read their keys at once (spec 24 §8)
};

struct SettingRow {
    Section section;
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

// The row's key in its section: the last part of its path.
constexpr std::string_view key(const SettingRow& row) noexcept {
    return row.path.substr(row.path.rfind('.') + 1);
}

// Every setting, in the order the original's file lists them.
std::span<const SettingRow> settings_table() noexcept;

// The row of the console command `name`, or null.
const SettingRow* find_command(std::string_view name) noexcept;

// Brings hand-edited or console-set values into range, in place: a pixel
// scale outside [kPixelScaleMin, kPixelScaleMax] (or not a number) becomes
// kPixelScaleDefault, and the snap rotation segments are rounded down to
// one of kSnapSegmentSteps (below the smallest: kSnapSegmentsDefault), and
// the comfort values are held to their ranges (spec 22 §3.1), and the
// streaming heap sizes to theirs (spec 24 §10). The
// runtime value Oculus becomes OpenXR (there is no LibOVR backend; Oculus
// means OpenXR). Run after every load and every mvr_* command.
// From: the guards in FUN_100202e0 (mvr_pixelscale) and FUN_10020870
// (the segment cascade).
void sanitize(Settings& cfg) noexcept;

// `scale` as the nearest of kTerminalTextScales; NaN is the default (spec
// 23 §8).
float terminal_text_step(float scale) noexcept;

// `segments` rounded down to one of kSnapSegmentSteps, or the default.
int snap_segments(int segments) noexcept;

// The names an enum has in mothervr.json, by value: every enum the file
// stores runs from 0 without gaps. `fallback` is what to_string gives a
// value outside them. Parsing is case-sensitive (spec 22 §3.1, 21 §15,
// 23 §8) and also takes the legacy spelling "Disabled" for VRRuntime::None.
template <class E>
struct EnumNames {
    static constexpr std::size_t kCapacity = 4;
    std::array<std::string_view, kCapacity> names{};
    E fallback{};

    constexpr std::size_t size() const noexcept {
        return static_cast<std::size_t>(
            std::ranges::count_if(names, [](std::string_view n) { return !n.empty(); }));
    }
};

template <class E>
inline constexpr EnumNames<E> kNames{};
template <>
inline constexpr EnumNames<VRRuntime> kNames<VRRuntime>{{"None", "Oculus", "OpenVR", "OpenXR"},
                                                        VRRuntime::OpenVR};
template <>
inline constexpr EnumNames<MirrorViewType> kNames<MirrorViewType>{{"BothEyes", "RightEye", "LeftEye"},
                                                                  MirrorViewType::RightEye};
template <>
inline constexpr EnumNames<StereoAA> kNames<StereoAA>{{"Auto", "On", "Off"}};
template <>
inline constexpr EnumNames<PlayMode> kNames<PlayMode>{{"Seated", "Standing"}};
template <>
inline constexpr EnumNames<FlashlightAim> kNames<FlashlightAim>{{"Head", "LeftHand", "RightHand"}};
template <>
inline constexpr EnumNames<BodyVisible> kNames<BodyVisible>{{"Full", "ArmsAndHands", "HandsOnly", "None"}};
template <>
inline constexpr EnumNames<CameraTakeovers> kNames<CameraTakeovers>{{"Game", "Reduced"}};
template <>
inline constexpr EnumNames<CrosshairMode> kNames<CrosshairMode>{{"On", "Off", "Auto"}};
template <>
inline constexpr EnumNames<HudPlacement> kNames<HudPlacement>{{"HeadLocked", "Follow"}};

// An enum the file stores by name.
template <class E>
concept NamedEnum = std::is_enum_v<E> && (kNames<E>.size() > 0);

// The last value of an enum the file stores.
template <NamedEnum E>
inline constexpr E kLast = static_cast<E>(kNames<E>.size() - 1);

// `e`'s name in the file.
template <NamedEnum E>
constexpr std::string_view to_string(E e) noexcept {
    const auto i = static_cast<std::size_t>(std::to_underlying(e));
    return kNames<E>.names[i < kNames<E>.size()
                               ? i
                               : static_cast<std::size_t>(std::to_underlying(kNames<E>.fallback))];
}

// The value named `s`, or none.
template <NamedEnum E>
constexpr std::optional<E> parse_enum(std::string_view s) noexcept {
    if constexpr (std::is_same_v<E, VRRuntime>) {
        if (s == "Disabled") return VRRuntime::None;
    }
    for (std::size_t i = 0; i < kNames<E>.size(); ++i) {
        if (kNames<E>.names[i] == s) return static_cast<E>(i);
    }
    return std::nullopt;
}

} // namespace omvr::core::config
