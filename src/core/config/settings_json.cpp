// mothervr.json load / save.
//
// From: FUN_10010750 @ 0x10010750 (load) and FUN_10014680 @ 0x10014680 (save).
// The original threads file I/O through std::ifstream/std::ofstream and lets
// nlohmann::json do the heavy lifting; we follow the same shape. Reading is
// per key (see settings.h for the layouts read); writing merges into the
// document last loaded so unknown keys survive. Every key comes from the
// settings table (settings_table.h).

#include "core/config/settings_json.h"

#include "core/config/settings_table.h"

#include <array>
#include <limits>
#include <type_traits>
#include <utility>

namespace omvr::core::config {

namespace {

using nlohmann::json;

// The object holding the settings.
constexpr const char* kSettingsKey = "Settings";

// The layout earlier builds of this project wrote: a top-level object
// "DebugOptions" holding "SkipStartupIntro" and "DevDebugOptions".
constexpr const char* kLegacyDebugKey     = "DebugOptions";
constexpr const char* kLegacySkipIntroKey = "SkipStartupIntro";
constexpr const char* kLegacyDevDebugKey  = "DevDebugOptions";

// Each Section's object, by Section value: its key (none for Settings
// itself) and the section it lies in.
struct SectionObject {
    const char* key;
    Section parent = Section::Settings;
};
constexpr std::array<SectionObject, std::to_underlying(Section::Hud) + 1> kSections{{
    {nullptr},
    {"Rendering"},
    {"Input"},
    {"Comfort"},
    {"Controls"},
    {"Hands"},
    {"Haptics", Section::Hands},
    {"Body"},
    {"Graphics"},
    // The v2 interaction and HUD pages (spec 23 §8).
    {"Interaction"},
    {"Hud"},
}};

constexpr const SectionObject& object_of(Section s) noexcept { return kSections[std::to_underlying(s)]; }
static_assert(std::string_view(object_of(Section::HandsHaptics).key) == "Haptics" &&
              std::string_view(object_of(Section::Hud).key) == "Hud");

// A JSON number as T. nlohmann's get<T> is a plain cast, undefined when the
// value lies outside T (a hand-edited 3e9 for an int, 1e39 for a float), so
// the value is first held to T's range; sanitize then applies the setting's
// own range, so a huge value ends at the setting's maximum.
template <class T>
T narrow_number(const json& v) {
    const auto d      = v.get<double>();
    constexpr auto lo = static_cast<double>(std::numeric_limits<T>::lowest());
    constexpr auto hi = static_cast<double>(std::numeric_limits<T>::max());
    if constexpr (std::is_integral_v<T>) {
        // Every int is a double exactly, so these compare without rounding.
        if (d <= lo) return std::numeric_limits<T>::lowest();
        if (d >= hi) return std::numeric_limits<T>::max();
    } else {
        if (d < lo) return std::numeric_limits<T>::lowest();
        if (d > hi) return std::numeric_limits<T>::max();
    }
    return static_cast<T>(d);
}

// Tolerant lookup: returns `fallback` if `key` is missing or has the wrong
// type, so older or hand-edited files load with defaults for what they lack.
template <class T>
T get_or(const json& j, std::string_view key, const T& fallback) {
    auto it = j.find(key);
    if (it == j.end()) return fallback;
    try {
        if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
            // get<T> reads a boolean as 0 or 1; anything else not a number
            // throws, as before.
            if (it->is_number()) return narrow_number<T>(*it);
        }
        return it->get<T>();
    } catch (...) {
        return fallback;
    }
}

// An enum stored as its string, or as the original's integer 0..last.
template <class E>
E get_enum_or(const json& j, std::string_view key, E fallback) {
    auto it = j.find(key);
    if (it == j.end()) return fallback;
    if (it->is_string()) return parse_enum<E>(it->get_ref<const std::string&>()).value_or(fallback);
    if (it->is_number_integer()) {
        const auto n = it->get<long long>();
        if (n >= 0 && n <= static_cast<long long>(kLast<E>)) return static_cast<E>(n);
    }
    return fallback;
}

// The object at `j[key]`, made one if it is missing or not an object.
json& object_at(json& j, const char* key) {
    json& v = j[key];
    if (!v.is_object()) v = json::object();
    return v;
}

// The section object of `settings` a row reads from, or null when it is
// missing or not an object.
const json* section_in(const json& settings, Section section) {
    const SectionObject& object = object_of(section);
    if (!object.key) return &settings;
    const json* within = section_in(settings, object.parent);
    if (!within) return nullptr;
    const auto it = within->find(object.key);
    return it != within->end() && it->is_object() ? &*it : nullptr;
}

// The section object of `settings` a row writes to, made where missing.
json& section_at(json& settings, Section section) {
    const SectionObject& object = object_of(section);
    if (!object.key) return settings;
    return object_at(section_at(settings, object.parent), object.key);
}

template <class T>
constexpr bool kIsEnum = std::is_enum_v<T>;

// Reads one row's value from `section` into `cfg`.
void read_row(const json& section, const SettingRow& row, Settings& cfg) {
    std::visit(
        [&](auto field) {
            auto& value = field(cfg);
            using T     = std::remove_reference_t<decltype(value)>;
            if constexpr (kIsEnum<T>) {
                value = get_enum_or(section, key(row), value);
            } else {
                value = get_or(section, key(row), value);
            }
        },
        row.field);
}

// One row's value as JSON: an enum as its string.
json value_of(const SettingRow& row, Settings& cfg) {
    return std::visit(
        [&](auto field) -> json {
            const auto& value = field(cfg);
            using T           = std::remove_cvref_t<decltype(value)>;
            if constexpr (kIsEnum<T>) {
                return to_string(value);
            } else {
                return value;
            }
        },
        row.field);
}

// The runtime as written: never "Oculus", which older tools would start a
// LibOVR backend from. Oculus means OpenXR here (spec 06 §1.1).
VRRuntime written_runtime(VRRuntime r) noexcept { return r == VRRuntime::Oculus ? VRRuntime::OpenXR : r; }

void read_legacy_debug(const json& root, Settings& c) {
    if (auto d = root.find(kLegacyDebugKey); d != root.end() && d->is_object()) {
        c.debug.skip_startup_intro = get_or(*d, kLegacySkipIntroKey, c.debug.skip_startup_intro);
        c.debug.debug              = get_or(*d, kLegacyDevDebugKey, c.debug.debug);
    }
}

} // namespace

Settings read_config(const json& root) {
    Settings c{};
    if (!root.is_object()) return c;

    // Our earlier layout first, so the original's keys win when both exist.
    read_legacy_debug(root, c);

    auto s = root.find(kSettingsKey);
    if (s == root.end() || !s->is_object()) return c;
    for (const SettingRow& row : settings_table()) {
        if (const json* section = section_in(*s, row.section)) read_row(*section, row, c);
    }
    return c;
}

void write_config(json& root, const Settings& c) {
    if (!root.is_object()) root = json::object();
    // Migrated into Settings; left in place it would override them on the
    // next read by an older build of ours.
    if (auto d = root.find(kLegacyDebugKey); d != root.end() && d->is_object()) root.erase(d);

    Settings written   = c;
    written.vr_runtime = written_runtime(c.vr_runtime);
    json& s            = object_at(root, kSettingsKey);
    for (const SettingRow& row : settings_table()) {
        section_at(s, row.section)[std::string(key(row))] = value_of(row, written);
    }
}

std::string describe(const Settings& c) {
    // Key paths as in mothervr.json, so a tester can hold the line against
    // the player's file.
    Settings shown = c;
    json j         = json::object();
    for (const SettingRow& row : settings_table()) j[std::string(row.path)] = value_of(row, shown);
    return j.dump();
}

} // namespace omvr::core::config
