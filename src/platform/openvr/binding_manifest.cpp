#include "platform/openvr/binding_manifest.h"

#include "platform/openvr/compat_names.h"
#include "platform/openvr/constants.h"
#include "platform/win32/text.h"

#include <windows.h>

#include <fstream>
#include <string_view>

namespace omvr::platform::openvr {

namespace {

using core::diag::ports::logf;
using nlohmann::json;

constexpr std::string_view kLogCannotWrite = "openvr input: cannot write {} (error {}), using legacy input";
constexpr std::string_view kLogWriteFailed = "openvr input: writing {} failed (error {}), using legacy input";

// ---- Default bindings ---------------------------------------------------------

json out(const char* action) { return {{"output", action_path(action)}}; }

json source(const std::string& path, const char* mode, json inputs, json parameters = nullptr) {
    json s = {{"path", path}, {"mode", mode}, {"inputs", std::move(inputs)}};
    if (!parameters.is_null()) s["parameters"] = std::move(parameters);
    return s;
}

std::string hand(const char* side, const char* input) {
    return std::string("/user/hand/") + side + "/input/" + input;
}

// The controls of a thumbstick controller (Index, Touch, WMR) that the
// default layout uses. A face button left null is not bound.
struct StickLayout {
    const char* stick;
    const char* grip_mode;
    const char* grip_input;
    const char* button_a;
    const char* button_b;
    const char* button_x;
    const char* button_y;
    std::string pause_path;
    const char* pause_mode;
};

// Thumbstick controllers: the layout of the original's Oculus Touch scheme,
// so its button glyphs fit.
json stick_sources(const StickLayout& l) {
    json s = json::array();
    s.push_back(
        source(hand("left", l.stick), "joystick", {{"position", out("move")}, {"click", out("sprint")}}));
    s.push_back(
        source(hand("right", l.stick), "joystick", {{"position", out("turn")}, {"click", out("radial")}}));
    s.push_back(source(hand("left", "trigger"), "trigger", {{"pull", out("crouch")}}));
    s.push_back(source(hand("right", "trigger"), "trigger", {{"pull", out("fire")}}));
    s.push_back(source(hand("left", "grip"), l.grip_mode, {{l.grip_input, out("tracker")}}));
    s.push_back(source(hand("right", "grip"), l.grip_mode, {{l.grip_input, out("aim")}}));
    if (l.button_a) s.push_back(source(hand("right", l.button_a), "button", {{"click", out("interact")}}));
    if (l.button_b) s.push_back(source(hand("right", l.button_b), "button", {{"click", out("reload")}}));
    if (l.button_x) s.push_back(source(hand("left", l.button_x), "button", {{"click", out("map")}}));
    if (l.button_y) s.push_back(source(hand("left", l.button_y), "button", {{"click", out("flashlight")}}));
    s.push_back(source(l.pause_path, l.pause_mode, {{"click", out("pause")}}));
    return s;
}

// The hand pose actions bound to `component` (/user/hand/<side>/pose/<component>).
//
// Which component: the game will treat these as OpenXR grip poses (the
// OpenXR backend binds /input/grip/pose), so pick SteamVR's closest match.
// SteamVR's own OpenXR runtime maps /input/grip/pose to the component
// /pose/openxr_grip (vrclient.dll pairs "/input/grip/pose" with
// "/pose/openxr_grip", as /input/aim/pose with /pose/openxr_aim), defined in
// the render model of controllers whose input profile lists it: Touch
// (touch_profile.json) does. Index, Vive wands and WMR profiles list no
// openxr_grip; there /pose/grip is the grip: the render models put it on
// the handle, in the palm, with the handgrip's tilt (Index: 13 cm behind
// and 1.5 cm below the raw pose's tip origin, rotated 15 deg about X), where
// raw and tip sit at the controller's nose and base at its bottom.
// "handgrip" has the same orientation but sits elsewhere on the handle
// (Index: 5 cm further forward, Vive: 1.8 cm higher), so grip is the better
// fit for a palm origin.
//
// The aim poses (21 §5.2, §16) go to the controller's pointing pose: the
// component SteamVR's OpenXR runtime pairs with /input/aim/pose,
// /pose/openxr_aim, where the input profile lists it (Touch), else
// /pose/tip, the controller's nose (Index, Vive wands, WMR, Steam Frame).
// Bound whatever Hands.Enabled is, so the setting applies at once (21 §15.1).
json hand_poses(const char* component, const char* aim_component) {
    json p   = json::array();
    auto add = [&p](const std::array<const char*, 2>& actions, const char* pose) {
        for (std::size_t h = 0; h < actions.size(); ++h) {
            p.push_back(
                {{"output", action_path(actions[h])},
                 {"path", std::string("/user/hand/") + (h == 0 ? "left" : "right") + "/pose/" + pose}});
        }
    };
    add(kPoseActions, component);
    add(kAimPoseActions, aim_component);
    return p;
}

// The vibration outputs (21 §14.3): each hand's action to its own
// controller's haptic output.
json haptic_outputs() {
    json o = json::array();
    for (std::size_t h = 0; h < kHapticActions.size(); ++h) {
        o.push_back({{"output", output_path(kHapticActions[h])},
                     {"path", std::string("/user/hand/") + (h == 0 ? "left" : "right") + "/output/haptic"}});
    }
    return o;
}

json binding_file(const char* controller, const char* description, json sources, const char* pose,
                  const char* aim_pose, bool haptics) {
    json set = {{"sources", std::move(sources)}, {"poses", hand_poses(pose, aim_pose)}};
    if (haptics) set["haptics"] = haptic_outputs();
    return {{"action_manifest_version", kManifestVersion},
            {"alias_info", json::object()},
            {"bindings", {{kSet, std::move(set)}}},
            {"controller_type", controller},
            {"description", description},
            {"name", std::string(kBindingNamePrefix) + description}};
}

// WMR: thumbsticks and touchpads, no face buttons. The right touchpad
// carries the touchpad scheme's interact/reload; the left one map and
// flashlight.
json wmr_sources() {
    json wmr = stick_sources({.stick      = "joystick",
                              .grip_mode  = "button",
                              .grip_input = "click",
                              .button_a   = nullptr,
                              .button_b   = nullptr,
                              .button_x   = nullptr,
                              .button_y   = nullptr,
                              .pause_path = hand("left", "application_menu"),
                              .pause_mode = "button"});
    wmr.push_back(source(hand("right", "trackpad"), "dpad",
                         {{"south", out("interact")}, {"north", out("reload")}}, {{"sub_mode", "click"}}));
    wmr.push_back(source(hand("left", "trackpad"), "dpad",
                         {{"south", out("map")}, {"north", out("flashlight")}}, {{"sub_mode", "click"}}));
    wmr.push_back(source(hand("right", "application_menu"), "button", {{"click", out("pause")}}));
    return wmr;
}

// Vive wands: the original touchpad scheme. Moving needs a touch, snap
// turning a click; the right pad's click regions interact (down), reload
// (up) and open the radial menu (centre).
json wand_sources() {
    json wand = json::array();
    wand.push_back(
        source(hand("left", "trackpad"), "trackpad", {{"position", out("move")}, {"click", out("sprint")}}));
    wand.push_back(source(hand("right", "trackpad"), "trackpad",
                          {{"position", out("turn")}, {"click", out("turn_engage")}}));
    wand.push_back(source(hand("right", "trackpad"), "dpad",
                          {{"south", out("interact")}, {"north", out("reload")}, {"center", out("radial")}},
                          {{"sub_mode", "click"}}));
    wand.push_back(source(hand("left", "trigger"), "trigger", {{"pull", out("crouch")}}));
    wand.push_back(source(hand("right", "trigger"), "trigger", {{"pull", out("fire")}}));
    wand.push_back(source(hand("left", "grip"), "button", {{"click", out("tracker")}}));
    wand.push_back(source(hand("right", "grip"), "button", {{"click", out("aim")}}));
    wand.push_back(source(hand("left", "application_menu"), "button", {{"click", out("map")}}));
    wand.push_back(source(hand("right", "application_menu"), "button", {{"click", out("pause")}}));
    return wand;
}

// Steam Frame (Valve's public input documentation): thumbsticks, A / B /
// X / Y and Menu on the right, a d-pad and a View button on the left, and a
// bumper above each trigger. The Touch layout where the buttons exist (right
// A interact, right B reload, the menu pauses); the left d-pad is the d-pad
// (menus, and the player's d-pad bindings in play, 22 §7); Map on the left
// View and Flashlight on the left bumper, where Touch has its left X / Y.
json frame_sources() {
    json frame = stick_sources({.stick      = "thumbstick",
                                .grip_mode  = "button",
                                .grip_input = "click",
                                .button_a   = "a",
                                .button_b   = "b",
                                .button_x   = "view",
                                .button_y   = "bumper",
                                .pause_path = hand("right", "menu"),
                                .pause_mode = "button"});
    frame.push_back(source(hand("left", "dpad_up"), "button", {{"click", out("dpad_up")}}));
    frame.push_back(source(hand("left", "dpad_down"), "button", {{"click", out("dpad_down")}}));
    frame.push_back(source(hand("left", "dpad_left"), "button", {{"click", out("dpad_left")}}));
    frame.push_back(source(hand("left", "dpad_right"), "button", {{"click", out("dpad_right")}}));
    return frame;
}

const char* type_name(Kind kind) noexcept {
    switch (kind) {
        case Kind::Boolean: return "boolean";
        case Kind::Vector1: return "vector1";
        case Kind::Vector2: return "vector2";
    }
    return "vector2";
}

} // namespace

std::string action_path(const char* name) { return std::string(kSet) + "/in/" + name; }

std::string output_path(const char* name) { return std::string(kSet) + "/out/" + name; }

std::string binding_file_name(const std::string& controller) {
    return kBindingFilePrefix + controller + kBindingFileSuffix;
}

std::vector<std::pair<std::string, json>> default_bindings(bool haptics) {
    const char* tip   = "tip";
    const char* touch = "openxr_aim";
    std::vector<std::pair<std::string, json>> files;

    // Index has no menu button: the left trackpad press pauses, and the left
    // A/B stand in for Touch's X/Y.
    files.emplace_back("knuckles", binding_file("knuckles", "Valve Index controllers",
                                                stick_sources({.stick      = "thumbstick",
                                                               .grip_mode  = "grab",
                                                               .grip_input = "grab",
                                                               .button_a   = "a",
                                                               .button_b   = "b",
                                                               .button_x   = "a",
                                                               .button_y   = "b",
                                                               .pause_path = hand("left", "trackpad"),
                                                               .pause_mode = "button"}),
                                                "grip", tip, haptics));

    files.emplace_back("oculus_touch",
                       binding_file("oculus_touch", "Oculus Touch",
                                    stick_sources({.stick      = "joystick",
                                                   .grip_mode  = "button",
                                                   .grip_input = "click",
                                                   .button_a   = "a",
                                                   .button_b   = "b",
                                                   .button_x   = "x",
                                                   .button_y   = "y",
                                                   .pause_path = hand("left", "application_menu"),
                                                   .pause_mode = "button"}),
                                    "openxr_grip", touch, haptics));

    files.emplace_back(
        "holographic_controller",
        binding_file("holographic_controller", "Windows Mixed Reality", wmr_sources(), "grip", tip, haptics));
    files.emplace_back("vive_controller",
                       binding_file("vive_controller", "Vive wands", wand_sources(), "grip", tip, haptics));
    files.emplace_back("frame_controller", binding_file("frame_controller", "Steam Frame controllers",
                                                        frame_sources(), "grip", tip, haptics));
    return files;
}

json manifest(const std::vector<std::pair<std::string, json>>& bindings, bool haptics) {
    json actions      = json::array();
    json localization = {{"language_tag", "en_US"}, {kSet, "Alien: Isolation"}};
    for (const auto& a : kActions) {
        actions.push_back({{"name", action_path(a.name)}, {"type", type_name(a.kind)}});
        localization[action_path(a.name)] = a.label;
    }
    for (std::size_t h = 0; h < kPoseActions.size(); ++h) {
        actions.push_back({{"name", action_path(kPoseActions[h])}, {"type", "pose"}});
        localization[action_path(kPoseActions[h])] = kPoseLabels[h];
    }
    for (std::size_t h = 0; h < kAimPoseActions.size(); ++h) {
        actions.push_back({{"name", action_path(kAimPoseActions[h])}, {"type", "pose"}});
        localization[action_path(kAimPoseActions[h])] = kAimPoseLabels[h];
    }
    for (std::size_t h = 0; haptics && h < kHapticActions.size(); ++h) {
        actions.push_back({{"name", output_path(kHapticActions[h])}, {"type", "vibration"}});
        localization[output_path(kHapticActions[h])] = kHapticLabels[h];
    }
    json defaults = json::array();
    for (const auto& [controller, _] : bindings) {
        defaults.push_back({{"controller_type", controller}, {"binding_url", binding_file_name(controller)}});
    }
    return {{"default_bindings", defaults},
            {"actions", actions},
            {"action_sets", json::array({{{"name", kSet}, {"usage", "single"}}})},
            {"localization", json::array({localization})}};
}

bool write_json(core::diag::ports::Log& log, const std::filesystem::path& path, const json& j) {
    std::ofstream f(path, std::ios::trunc);
    if (!f) {
        const DWORD error = ::GetLastError();
        logf(log, kLogCannotWrite, win32::utf8(path), error);
        return false;
    }
    f << j.dump(kJsonIndent);
    f.flush();
    if (!f.good()) {
        const DWORD error = ::GetLastError();
        logf(log, kLogWriteFailed, win32::utf8(path), error);
        return false;
    }
    return true;
}

} // namespace omvr::platform::openvr
