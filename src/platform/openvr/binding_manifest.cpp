#include "platform/openvr/binding_manifest.h"

#include "core/diag/logger.h" // R: vr (A1): log through an injected Log&
#include "platform/openvr/constants.h"
#include "platform/win32/text.h"

#include <windows.h>

#include <fstream>

namespace omvr::platform::openvr {

namespace {

namespace diag {
using core::diag::logf;
using win32::utf8;
} // namespace diag

using nlohmann::json;

} // namespace

std::string action_path(const char* name) { return std::string(kSet) + "/in/" + name; }

namespace {

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

// Thumbstick controllers (Index, Touch, WMR): the layout of the original's
// Oculus Touch scheme, so its button glyphs fit.
json stick_sources(const char* stick, const char* grip_mode, const char* grip_input,
                   const char* button_a, const char* button_b, const char* button_x,
                   const char* button_y, const std::string& pause_path, const char* pause_mode) {
    json s = json::array();
    s.push_back(source(hand("left", stick), "joystick",
                       {{"position", out("move")}, {"click", out("sprint")}}));
    s.push_back(source(hand("right", stick), "joystick",
                       {{"position", out("turn")}, {"click", out("radial")}}));
    s.push_back(source(hand("left", "trigger"), "trigger", {{"pull", out("crouch")}}));
    s.push_back(source(hand("right", "trigger"), "trigger", {{"pull", out("fire")}}));
    s.push_back(source(hand("left", "grip"), grip_mode, {{grip_input, out("tracker")}}));
    s.push_back(source(hand("right", "grip"), grip_mode, {{grip_input, out("aim")}}));
    if (button_a) s.push_back(source(hand("right", button_a), "button", {{"click", out("interact")}}));
    if (button_b) s.push_back(source(hand("right", button_b), "button", {{"click", out("reload")}}));
    if (button_x) s.push_back(source(hand("left", button_x), "button", {{"click", out("map")}}));
    if (button_y) s.push_back(source(hand("left", button_y), "button", {{"click", out("flashlight")}}));
    s.push_back(source(pause_path, pause_mode, {{"click", out("pause")}}));
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
json hand_poses(const char* component) {
    json p = json::array();
    for (int h = 0; h < 2; ++h) {
        p.push_back({{"output", action_path(kPoseActions[h])},
                     {"path", std::string("/user/hand/") + (h == 0 ? "left" : "right") + "/pose/" + component}});
    }
    return p;
}

json binding_file(const char* controller, const char* description, json sources, const char* pose) {
    return {{"action_manifest_version", kManifestVersion},
            {"alias_info", json::object()},
            {"bindings", {{kSet, {{"sources", std::move(sources)}, {"poses", hand_poses(pose)}}}}},
            {"controller_type", controller},
            {"description", description},
            {"name", std::string("MotherVR revived: ") + description}};
}

} // namespace

std::vector<std::pair<std::string, json>> default_bindings() {
    std::vector<std::pair<std::string, json>> files;

    files.emplace_back("knuckles", binding_file("knuckles", "Valve Index controllers",
        stick_sources("thumbstick", "grab", "grab", "a", "b", "a", "b",
                      hand("left", "trackpad"), "button"), "grip"));
    // Index has no menu button: the left trackpad press pauses, and the left
    // A/B stand in for Touch's X/Y.

    files.emplace_back("oculus_touch", binding_file("oculus_touch", "Oculus Touch",
        stick_sources("joystick", "button", "click", "a", "b", "x", "y",
                      hand("left", "application_menu"), "button"), "openxr_grip"));

    // WMR: thumbsticks and touchpads, no face buttons. The right touchpad
    // carries the touchpad scheme's interact/reload; the left one map and
    // flashlight.
    json wmr = stick_sources("joystick", "button", "click", nullptr, nullptr, nullptr, nullptr,
                             hand("left", "application_menu"), "button");
    wmr.push_back(source(hand("right", "trackpad"), "dpad",
                         {{"south", out("interact")}, {"north", out("reload")}},
                         {{"sub_mode", "click"}}));
    wmr.push_back(source(hand("left", "trackpad"), "dpad",
                         {{"south", out("map")}, {"north", out("flashlight")}},
                         {{"sub_mode", "click"}}));
    wmr.push_back(source(hand("right", "application_menu"), "button", {{"click", out("pause")}}));
    files.emplace_back("holographic_controller",
                       binding_file("holographic_controller", "Windows Mixed Reality", std::move(wmr), "grip"));

    // Vive wands: the original touchpad scheme. Moving needs a touch,
    // snap turning a click; the right pad's click regions interact (down),
    // reload (up) and open the radial menu (centre).
    json wand = json::array();
    wand.push_back(source(hand("left", "trackpad"), "trackpad",
                          {{"position", out("move")}, {"click", out("sprint")}}));
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
    files.emplace_back("vive_controller", binding_file("vive_controller", "Vive wands", std::move(wand), "grip"));
    return files;
}

json manifest(const std::vector<std::pair<std::string, json>>& bindings) {
    json actions = json::array();
    json localization = {{"language_tag", "en_US"}, {kSet, "Alien: Isolation"}};
    for (const auto& a : kActions) {
        const char* type = a.kind == Kind::Boolean ? "boolean" : a.kind == Kind::Vector1 ? "vector1" : "vector2";
        actions.push_back({{"name", action_path(a.name)}, {"type", type}});
        localization[action_path(a.name)] = a.label;
    }
    for (std::size_t h = 0; h < kPoseActions.size(); ++h) {
        actions.push_back({{"name", action_path(kPoseActions[h])}, {"type", "pose"}});
        localization[action_path(kPoseActions[h])] = kPoseLabels[h];
    }
    json defaults = json::array();
    for (const auto& [controller, _] : bindings) {
        defaults.push_back({{"controller_type", controller}, {"binding_url", "bindings_" + controller + ".json"}});
    }
    return {{"default_bindings", defaults},
            {"actions", actions},
            {"action_sets", json::array({{{"name", kSet}, {"usage", "single"}}})},
            {"localization", json::array({localization})}};
}

bool write_json(const std::filesystem::path& path, const json& j) {
    std::ofstream f(path, std::ios::trunc);
    if (!f) {
        const DWORD error = ::GetLastError();
        diag::logf("openvr input: cannot write {} (error {}), using legacy input", diag::utf8(path), error);
        return false;
    }
    f << j.dump(2);
    f.flush();
    if (!f.good()) {
        const DWORD error = ::GetLastError();
        diag::logf("openvr input: writing {} failed (error {}), using legacy input", diag::utf8(path), error);
        return false;
    }
    return true;
}

} // namespace omvr::platform::openvr
