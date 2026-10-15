// mothervr.json load / save.
//
// From: FUN_10010750 @ 0x10010750 (load) and FUN_10014680 @ 0x10014680 (save).
// The original threads file I/O through std::ifstream/std::ofstream and lets
// nlohmann::json do the heavy lifting; we follow the same shape. Reading is
// per key (see settings.h for the layouts read); writing merges into the
// document last loaded so unknown keys survive.

#include "core/config/settings_json.h"

#include "core/config/settings_table.h"

namespace omvr::core::config {

namespace {

using nlohmann::json;
using Config = Settings;

constexpr const char* kKey_Settings           = "Settings";
constexpr const char* kKey_VRRuntime          = "VRRuntime";
constexpr const char* kKey_Rendering          = "Rendering";
constexpr const char* kKey_MirrorViewType     = "MirrorViewType";
constexpr const char* kKey_SymmetricMirrorView = "SymmetricMirrorView";
constexpr const char* kKey_PixelScale         = "PixelScale";
constexpr const char* kKey_ClippingBlinders   = "ClippingBlinders";
constexpr const char* kKey_StereoAntiAliasing = "StereoAntiAliasing";
constexpr const char* kKey_Input              = "Input";
constexpr const char* kKey_SnapRotation       = "SnapRotation";
constexpr const char* kKey_SnapRotationSegments = "SnapRotationSegments";
constexpr const char* kKey_VRInputRelativeWalking = "VRInputRelativeWalking";
constexpr const char* kKey_VRInputStrafing    = "VRInputStrafing";
constexpr const char* kKey_ToggleGrips        = "ToggleGrips";
constexpr const char* kKey_ToggleCrouch       = "ToggleCrouch";
constexpr const char* kKey_DebugOptions       = "DebugOptions";      // Settings: bool; legacy top level: object
constexpr const char* kKey_SkipStartupIntro   = "SkipStartupIntro";
constexpr const char* kKey_DevDebugOptions    = "DevDebugOptions";   // legacy, inside the top-level object

// Tolerant lookup: returns `fallback` if `key` is missing or has the wrong
// type, so older or hand-edited files load with defaults for what they lack.
template <class T>
T get_or(const json& j, const char* key, const T& fallback) {
    auto it = j.find(key);
    if (it == j.end()) return fallback;
    try { return it->get<T>(); } catch (...) { return fallback; }
}

// An enum stored as its string, or as the original's integer 0..`last`.
template <class E>
E get_enum_or(const json& j, const char* key, E fallback,
              std::optional<E> (*parse)(std::string_view) noexcept, E last) {
    auto it = j.find(key);
    if (it == j.end()) return fallback;
    if (it->is_string()) return parse(it->get_ref<const std::string&>()).value_or(fallback);
    if (it->is_number_integer()) {
        const auto n = it->get<long long>();
        if (n >= 0 && n <= static_cast<long long>(last)) return static_cast<E>(n);
    }
    return fallback;
}

// The object at `j[key]`, made one if it is missing or not an object.
json& object_at(json& j, const char* key) {
    json& v = j[key];
    if (!v.is_object()) v = json::object();
    return v;
}

} // namespace

namespace {

void read_rendering(const json& j, RenderingSettings& r) {
    r.mirror_view_type      = get_enum_or(j, kKey_MirrorViewType, r.mirror_view_type,
                                          &parse_mirror_view_type, MirrorViewType::LeftEye);
    r.symmetric_mirror_view = get_or(j, kKey_SymmetricMirrorView, r.symmetric_mirror_view);
    r.pixel_scale           = get_or(j, kKey_PixelScale, r.pixel_scale);
    r.clipping_blinders     = get_or(j, kKey_ClippingBlinders, r.clipping_blinders);
    r.stereo_aa             = get_enum_or(j, kKey_StereoAntiAliasing, r.stereo_aa, &parse_stereo_aa, StereoAA::Off);
}

void read_input(const json& j, InputSettings& i) {
    i.snap_rotation             = get_or(j, kKey_SnapRotation, i.snap_rotation);
    i.snap_rotation_segments    = get_or(j, kKey_SnapRotationSegments, i.snap_rotation_segments);
    i.vr_input_relative_walking = get_or(j, kKey_VRInputRelativeWalking, i.vr_input_relative_walking);
    i.vr_input_strafing         = get_or(j, kKey_VRInputStrafing, i.vr_input_strafing);
    i.toggle_grips              = get_or(j, kKey_ToggleGrips, i.toggle_grips);
    i.toggle_crouch             = get_or(j, kKey_ToggleCrouch, i.toggle_crouch);
}

} // namespace

Config read_config(const json& root) {
    Config c{};
    if (!root.is_object()) return c;

    // Our earlier layout first, so the original's keys win when both exist.
    if (auto d = root.find(kKey_DebugOptions); d != root.end() && d->is_object()) {
        c.debug.skip_startup_intro = get_or(*d, kKey_SkipStartupIntro, c.debug.skip_startup_intro);
        c.debug.debug              = get_or(*d, kKey_DevDebugOptions, c.debug.debug);
    }

    auto s = root.find(kKey_Settings);
    if (s == root.end() || !s->is_object()) return c;
    c.vr_runtime = get_enum_or(*s, kKey_VRRuntime, c.vr_runtime, &parse_vr_runtime, VRRuntime::OpenXR);
    if (auto r = s->find(kKey_Rendering); r != s->end() && r->is_object()) read_rendering(*r, c.rendering);
    if (auto in = s->find(kKey_Input); in != s->end() && in->is_object()) read_input(*in, c.input);
    c.debug.debug              = get_or(*s, kKey_DebugOptions, c.debug.debug);
    c.debug.skip_startup_intro = get_or(*s, kKey_SkipStartupIntro, c.debug.skip_startup_intro);
    return c;
}

// Writes `c` into `root` in the original's layout, keeping other keys.
void write_config(json& root, const Config& c) {
    if (!root.is_object()) root = json::object();
    // Migrated into Settings; left in place it would override them on the
    // next read by an older build of ours.
    if (auto d = root.find(kKey_DebugOptions); d != root.end() && d->is_object()) root.erase(d);

    json& s = object_at(root, kKey_Settings);
    // Never "Oculus": older tools would start a LibOVR backend from it.
    // Oculus means OpenXR here (spec 06 §1.1).
    s[kKey_VRRuntime] = to_string(c.vr_runtime == VRRuntime::Oculus ? VRRuntime::OpenXR : c.vr_runtime);

    json& r = object_at(s, kKey_Rendering);
    r[kKey_MirrorViewType]      = to_string(c.rendering.mirror_view_type);
    r[kKey_SymmetricMirrorView] = c.rendering.symmetric_mirror_view;
    r[kKey_PixelScale]          = c.rendering.pixel_scale;
    r[kKey_ClippingBlinders]    = c.rendering.clipping_blinders;
    r[kKey_StereoAntiAliasing]  = to_string(c.rendering.stereo_aa);

    json& in = object_at(s, kKey_Input);
    in[kKey_SnapRotation]           = c.input.snap_rotation;
    in[kKey_SnapRotationSegments]   = c.input.snap_rotation_segments;
    in[kKey_VRInputRelativeWalking] = c.input.vr_input_relative_walking;
    in[kKey_VRInputStrafing]        = c.input.vr_input_strafing;
    in[kKey_ToggleGrips]            = c.input.toggle_grips;
    in[kKey_ToggleCrouch]           = c.input.toggle_crouch;

    s[kKey_DebugOptions]     = c.debug.debug;
    s[kKey_SkipStartupIntro] = c.debug.skip_startup_intro;
}

std::string describe(const Config& c) {
    // Key paths as in mothervr.json, so a tester can hold the line against
    // the player's file.
    json j = json::object();
    j["VRRuntime"]                     = to_string(c.vr_runtime);
    j["Rendering.MirrorViewType"]      = to_string(c.rendering.mirror_view_type);
    j["Rendering.SymmetricMirrorView"] = c.rendering.symmetric_mirror_view;
    j["Rendering.PixelScale"]          = c.rendering.pixel_scale;
    j["Rendering.ClippingBlinders"]    = c.rendering.clipping_blinders;
    j["Rendering.StereoAntiAliasing"]  = to_string(c.rendering.stereo_aa);
    j["Input.SnapRotation"]            = c.input.snap_rotation;
    j["Input.SnapRotationSegments"]    = c.input.snap_rotation_segments;
    j["Input.VRInputRelativeWalking"]  = c.input.vr_input_relative_walking;
    j["Input.VRInputStrafing"]         = c.input.vr_input_strafing;
    j["Input.ToggleGrips"]             = c.input.toggle_grips;
    j["Input.ToggleCrouch"]            = c.input.toggle_crouch;
    j["DebugOptions"]                  = c.debug.debug;
    j["SkipStartupIntro"]              = c.debug.skip_startup_intro;
    return j.dump();
}

} // namespace omvr::core::config
