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

#include <optional>
#include <type_traits>

namespace omvr::core::config {

namespace {

using nlohmann::json;

// The object holding the settings, and the objects of its sections.
constexpr const char* kSettingsKey  = "Settings";
constexpr const char* kRenderingKey = "Rendering";
constexpr const char* kInputKey     = "Input";
constexpr const char* kComfortKey   = "Comfort";
constexpr const char* kControlsKey  = "Controls";
constexpr const char* kHandsKey     = "Hands";
constexpr const char* kHapticsKey   = "Haptics";
constexpr const char* kBodyKey      = "Body";

// The layout earlier builds of this project wrote: a top-level object
// "DebugOptions" holding "SkipStartupIntro" and "DevDebugOptions".
constexpr const char* kLegacyDebugKey     = "DebugOptions";
constexpr const char* kLegacySkipIntroKey = "SkipStartupIntro";
constexpr const char* kLegacyDevDebugKey  = "DevDebugOptions";

const char* section_key(Section s) noexcept {
    switch (s) {
        case Section::Rendering: return kRenderingKey;
        case Section::Input: return kInputKey;
        case Section::Comfort: return kComfortKey;
        case Section::Controls: return kControlsKey;
        case Section::Hands: return kHandsKey;
        case Section::HandsHaptics: return kHapticsKey;
        case Section::Body: return kBodyKey;
        case Section::Settings: break;
    }
    return nullptr;
}

// The section a section lies in, or nullopt for one directly in Settings.
std::optional<Section> parent_of(Section s) noexcept {
    if (s == Section::HandsHaptics) return Section::Hands;
    return std::nullopt;
}

// Tolerant lookup: returns `fallback` if `key` is missing or has the wrong
// type, so older or hand-edited files load with defaults for what they lack.
template <class T>
T get_or(const json& j, std::string_view key, const T& fallback) {
    auto it = j.find(key);
    if (it == j.end()) return fallback;
    try {
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
    if (it->is_string()) return EnumTraits<E>::parse(it->get_ref<const std::string&>()).value_or(fallback);
    if (it->is_number_integer()) {
        const auto n = it->get<long long>();
        if (n >= 0 && n <= static_cast<long long>(EnumTraits<E>::kLast)) return static_cast<E>(n);
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
    const char* key = section_key(section);
    if (!key) return &settings;
    const auto parent  = parent_of(section);
    const json* within = parent ? section_in(settings, *parent) : &settings;
    if (!within) return nullptr;
    const auto it = within->find(key);
    return it != within->end() && it->is_object() ? &*it : nullptr;
}

// The section object of `settings` a row writes to, made where missing.
json& section_at(json& settings, Section section) {
    const char* key = section_key(section);
    if (!key) return settings;
    const auto parent = parent_of(section);
    return object_at(parent ? section_at(settings, *parent) : settings, key);
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
                value = get_enum_or(section, row.key, value);
            } else {
                value = get_or(section, row.key, value);
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
        section_at(s, row.section)[std::string(row.key)] = value_of(row, written);
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
