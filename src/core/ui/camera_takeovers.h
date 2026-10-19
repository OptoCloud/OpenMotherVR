// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"

#include <cstdint>
#include <optional>
#include <string_view>

// Interaction.CameraTakeovers (spec 23 §4): which of the game's scripted
// turns of the first-person camera are kept. With "Reduced" the steer of
// the StealCamera start and of the CameraResource stop, and the yaw
// re-align to the body of CameraResetRequest (mode 6), of the player state
// entry and of the CameraResource stop, are not applied (23 §4.2 (b), (c));
// level cameras, convergence and every takeover during a cutscene or level
// fade stay the game's (23 §4.2, §4.4). An interaction in progress keeps
// the rule it started with (23 §8). Part (a), the animation-driven view of
// the short device animations, waits for 23 §12 items 5 and 12 and is not
// decided here. Game thread.
namespace omvr::core::ui {

// The game's takeovers this rule covers, one per call site (23 §2.4).
enum class Takeover : std::uint8_t {
    StealCameraSteer,       // the StealCamera start turns the view to a point
    LevelCameraExitSteer,   // the CameraResource stop turns the view back
    LevelCameraExitRealign, // the same stop, reset_player_camera_on_exit
    ResetRealign,           // CameraResetRequest, mode 6
    StateEntryRealign,      // the player state entry
};

// The takeover's name for the log.
[[nodiscard]] std::string_view takeover_name(Takeover kind) noexcept;

// What the rule depends on, read from the game and the settings.
struct TakeoverInputs {
    config::CameraTakeovers setting = config::CameraTakeovers::Game;
    bool in_interaction             = false; // between an interaction's start and its end (11 §5.3)
    bool cutscene_or_fade           = false; // 05 §3.2g
};

class CameraTakeoverRule final {
public:
    // Once per stereo update and before each decision: an interaction that
    // has begun takes the setting of that moment as its rule, until it
    // ends.
    void observe(const TakeoverInputs& in) noexcept;

    // The rule in effect: the setting, or the one the interaction in
    // progress started with.
    [[nodiscard]] config::CameraTakeovers rule(const TakeoverInputs& in) const noexcept;

    // Whether the game's takeover is to be skipped now. Calls observe.
    [[nodiscard]] bool suppress(const TakeoverInputs& in) noexcept;

private:
    std::optional<config::CameraTakeovers> interaction_rule_;
};

} // namespace omvr::core::ui
