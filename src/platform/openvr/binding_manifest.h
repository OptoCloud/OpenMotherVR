#pragma once

#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// The SteamVR Input action manifest and default bindings per controller
// type, written next to the game; players can rebind everything in
// SteamVR's binding UI.
namespace omvr::platform::openvr {

// The semantic game actions, in manifest order.
enum class Action : std::size_t {
    Move, Turn, TurnEngage, Sprint, Crouch, Fire, Aim, Tracker,
    Interact, Reload, Radial, Flashlight, Map, Pause,
    Count
};

inline constexpr const char* kSet = "/actions/main";

enum class Kind { Boolean, Vector1, Vector2 };
struct ActionDef {
    Action id;
    const char* name;
    Kind kind;
    const char* label;
};

// Semantic game actions, indexed by Action. The default bindings decide
// which control drives each; players can change them in SteamVR.
inline constexpr std::array kActions = {
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
inline constexpr std::array<const char*, 2> kPoseActions = {"hand_left", "hand_right"};
inline constexpr std::array<const char*, 2> kPoseLabels  = {"Left hand", "Right hand"};

inline constexpr std::size_t index(Action a) noexcept { return static_cast<std::size_t>(a); }

std::string action_path(const char* name);

// The default binding file of each controller type, by controller name.
std::vector<std::pair<std::string, nlohmann::json>> default_bindings();

// The action manifest naming `bindings` as the defaults.
nlohmann::json manifest(const std::vector<std::pair<std::string, nlohmann::json>>& bindings);

// False, logged with the Windows error, when the file cannot be written: the
// caller then falls back to the legacy controller path, and the log must say
// why (04 §5.8.2, 11 §2.5).
bool write_json(const std::filesystem::path& path, const nlohmann::json& j);

} // namespace omvr::platform::openvr
