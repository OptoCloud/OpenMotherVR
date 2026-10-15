#include "openvr_actions.h"

#include "diag/log.h"
#include "openvr_backend.h"

#include <nlohmann/json.hpp>
#include <openvr.h>
#include <windows.h>

#include <array>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace mothervr::runtime::openvr {

namespace {

using nlohmann::json;

constexpr const char* kSet = "/actions/main";

enum class Kind { Boolean, Vector1, Vector2 };
struct ActionDef {
    Action id;
    const char* name;
    Kind kind;
    const char* label;
};

// Semantic game actions, indexed by Action. The default bindings below
// decide which control drives each; players can change them in SteamVR.
constexpr std::array kActions = {
    ActionDef{Action::Move, "move", Kind::Vector2, "Move"},
    ActionDef{Action::Turn, "turn", Kind::Vector2, "Turn / weapon select"},
    ActionDef{Action::TurnEngage, "turn_engage", Kind::Boolean, "Turn (touchpad click)"},
    ActionDef{Action::Sprint, "sprint", Kind::Boolean, "Sprint"},
    ActionDef{Action::Crouch, "crouch", Kind::Vector1, "Crouch"},
    ActionDef{Action::Fire, "fire", Kind::Vector1, "Fire / throw / melee"},
    ActionDef{Action::Aim, "aim", Kind::Boolean, "Aim"},
    ActionDef{Action::Tracker, "tracker", Kind::Boolean, "Motion tracker"},
    ActionDef{Action::Interact, "interact", Kind::Boolean, "Interact (near head: flashlight)"},
    ActionDef{Action::Reload, "reload", Kind::Boolean, "Reload / activate / back"},
    ActionDef{Action::Radial, "radial", Kind::Boolean, "Radial menu"},
    ActionDef{Action::Flashlight, "flashlight", Kind::Boolean, "Flashlight"},
    ActionDef{Action::Map, "map", Kind::Boolean, "Map"},
    ActionDef{Action::Pause, "pause", Kind::Boolean, "Pause"},
};
static_assert(kActions.size() == static_cast<std::size_t>(Action::Count));
static_assert([] {
    for (std::size_t i = 0; i < kActions.size(); ++i)
        if (static_cast<std::size_t>(kActions[i].id) != i) return false;
    return true;
}(), "kActions must be in Action order");

// The hands' grip poses: one pose action per hand, so neither depends on
// which controller SteamVR thinks is which (restrict-to-device unused).
constexpr std::array<const char*, 2> kPoseActions = {"hand_left", "hand_right"};
constexpr std::array<const char*, 2> kPoseLabels  = {"Left hand", "Right hand"};

// Bumped whenever actions are added (2: the hand poses). SteamVR reads the
// manifest we rewrite each launch, so new actions and the default bindings
// for them arrive by themselves; the number marks which manifest a binding
// file was made for, so a binding a player saved from an older version can
// be told apart in SteamVR's binding UI (it lacks the new actions, which
// then read inactive: see hand_poses()).
constexpr int kManifestVersion = 2;

constexpr std::size_t index(Action a) noexcept { return static_cast<std::size_t>(a); }

std::string action_path(const char* name) { return std::string(kSet) + "/in/" + name; }

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

// False, logged with the Windows error, when the file cannot be written: the
// caller then falls back to the legacy controller path, and the log must say
// why (04 §5.8.2, 11 §2.5).
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

} // namespace

// ---- Runtime ----------------------------------------------------------------------

bool Actions::digital(Action a) noexcept {
    vr::InputDigitalActionData_t d{};
    return input_->GetDigitalActionData(handles_[index(a)], &d, sizeof(d), vr::k_ulInvalidInputValueHandle) ==
               vr::VRInputError_None && d.bActive && d.bState;
}

Actions::Analog Actions::analog(Action a) noexcept {
    vr::InputAnalogActionData_t d{};
    if (input_->GetAnalogActionData(handles_[index(a)], &d, sizeof(d), vr::k_ulInvalidInputValueHandle) !=
        vr::VRInputError_None || !d.bActive) {
        return {};
    }
    return {d.x, d.y};
}

bool Actions::init(const std::filesystem::path& dir) noexcept {
    try {
        auto* input = static_cast<vr::IVRInput*>(generic_interface(vr::IVRInput_Version));
        if (!input) {
            diag::log_line("openvr input: IVRInput unavailable, using legacy input");
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const auto bindings = default_bindings();
        for (const auto& [controller, file] : bindings) {
            if (!write_json(dir / ("bindings_" + controller + ".json"), file)) return false;
        }
        const auto manifest_path = dir / "actions.json";
        if (!write_json(manifest_path, manifest(bindings))) return false;

        // OpenVR takes UTF-8 paths.
        const auto err = input->SetActionManifestPath(diag::utf8(manifest_path).c_str());
        if (err != vr::VRInputError_None) {
            diag::logf("openvr input: SetActionManifestPath failed ({}), using legacy input", static_cast<int>(err));
            return false;
        }
        input->GetActionSetHandle(kSet, &set_);
        for (const auto& a : kActions) {
            input->GetActionHandle(action_path(a.name).c_str(), &handles_[index(a.id)]);
        }
        for (std::size_t h = 0; h < kPoseActions.size(); ++h) {
            input->GetActionHandle(action_path(kPoseActions[h]).c_str(), &pose_handles_[h]);
        }
        input_ = input;
        diag::logf("openvr input: SteamVR Input with {} actions + 2 hand poses, manifest {}", kActions.size(),
                   diag::utf8(manifest_path));
        return true;
    } catch (const std::exception& e) {
        diag::logf("openvr input: {}", e.what());
        return false;
    }
}

Controls Actions::poll() noexcept {
    Controls c;
    if (!input_) return c;
    vr::VRActiveActionSet_t active{};
    active.ulActionSet = set_;
    if (input_->UpdateActionState(&active, sizeof(active), 1) != vr::VRInputError_None) return c;

    const auto move = analog(Action::Move);
    const auto turn = analog(Action::Turn);
    c.move = {move.x, move.y};
    c.turn = {turn.x, turn.y};
    c.trigger_l = analog(Action::Crouch).x;
    c.trigger_r = analog(Action::Fire).x;
    c.grip_l = digital(Action::Tracker);
    c.grip_r = digital(Action::Aim);
    c.sprint = digital(Action::Sprint);
    c.interact = digital(Action::Interact);
    c.reload = digital(Action::Reload);
    c.radial = digital(Action::Radial);
    c.map = digital(Action::Map);
    c.flashlight = digital(Action::Flashlight);
    c.pause = digital(Action::Pause);

    // A bound turn_engage (touchpad click) means the touchpad scheme.
    vr::InputDigitalActionData_t engage{};
    input_->GetDigitalActionData(handles_[index(Action::TurnEngage)], &engage, sizeof(engage),
                                 vr::k_ulInvalidInputValueHandle);
    c.touchpad_scheme = engage.bActive;
    c.turn_engaged = !engage.bActive || engage.bState;
    return c;
}

std::array<HandPose, 2> Actions::hand_poses(int origin) noexcept {
    std::array<HandPose, 2> out{};
    if (!input_) return out;
    bool unbound = false;
    for (std::size_t h = 0; h < out.size(); ++h) {
        vr::InputPoseActionData_t d{};
        if (input_->GetPoseActionDataForNextFrame(pose_handles_[h], static_cast<vr::ETrackingUniverseOrigin>(origin),
                                                  &d, sizeof(d), vr::k_ulInvalidInputValueHandle) !=
            vr::VRInputError_None) {
            continue;
        }
        if (!d.bActive) {
            unbound = true;
            continue;
        }
        if (!d.pose.bPoseIsValid) continue;
        auto& p = out[h];
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c) p.m[r][c] = d.pose.mDeviceToAbsoluteTracking.m[r][c];
        for (int i = 0; i < 3; ++i) p.velocity[i] = d.pose.vVelocity.v[i];
        p.valid = true;
    }
    // Controllers bound (fire reads active) but a hand pose not: a binding
    // saved from a manifest before the hand poses existed.
    if (unbound && !unbound_logged_) {
        vr::InputAnalogActionData_t fire{};
        if (input_->GetAnalogActionData(handles_[index(Action::Fire)], &fire, sizeof(fire),
                                        vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
            fire.bActive) {
            unbound_logged_ = true;
            diag::log_line("openvr input: hand pose actions unbound; a custom SteamVR binding for this game "
                           "from an older version lacks them (reset it to the default binding)");
        }
    }
    return out;
}

} // namespace mothervr::runtime::openvr
