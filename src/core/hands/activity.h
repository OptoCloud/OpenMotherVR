// OpenMotherVR-Provenance: clean
#pragma once

#include <cstdint>
#include <string_view>

// When the tracked hands and the features built on them are active (spec 21
// §4.4, §12.2): only in first-person play the player controls.
namespace omvr::core::hands {

// What the game thread knows each frame, from the signals v1 already has
// (21 §2.10).
struct ActivityInput {
    bool enabled          = false; // Hands.Enabled
    bool stereo_running   = false; // the stereo device runs (state 2)
    bool world_mode       = false; // stereo mode 0, not the virtual screen
    bool player           = false; // the player character exists (21 §2.2)
    bool alive            = false; // its health is above 0
    bool follow_active    = false; // a FollowBehavior is tracked and active (05 §6.1)
    bool cutscene_or_fade = false; // 05 §6.2, §3.2g
    bool menu_screen      = false; // a loading, pause or title screen is up (05 §5)
    bool interaction      = false; // an interaction is in progress (11 §5.3)
    bool hiding           = false;
    bool player_action    = false; // r11: a player action episode runs (21 §12.1)
};

// Why the hands are not active, or Active.
enum class Activity : std::uint8_t {
    Active,
    Off,         // Hands.Enabled off
    NoStereo,    // no running stereo device
    Screen,      // menus, videos: the virtual screen
    NoPlayer,    // no player character
    Dead,        // health 0
    CameraTaken, // no active first-person camera
    Cutscene,    // cutscene or level fade
    MenuScreen,  // loading, pause, title
    Interaction, // r1: no interaction kind joins the hands (21 §4.4)
    Hiding,
    PlayerAction, // r11: a grab, trap, melee kill or hit (21 §12.1)
};

// 21 §4.4: active only while every condition holds; the first that does not
// is the reason.
Activity activity(const ActivityInput& in) noexcept;

// The reason as the log says it (21 §18).
std::string_view to_string(Activity a) noexcept;

// An episode of 21 §12.1 (logged at DBG) rather than a setting or the
// player's own state (INF).
bool is_episode(Activity a) noexcept;

} // namespace omvr::core::hands
