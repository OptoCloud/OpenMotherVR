// OpenMotherVR-Provenance: clean
#pragma once

#include "core/ui/types.h"

#include <array>
#include <atomic>
#include <cstdint>

// The player's "HUD hidden" state (spec 22 §8.3, 23 §7.2) and what it does
// to the game's two HUD objects. The state starts each session as
// Hud.Hidden says, follows the setting when the options page changes it,
// and is flipped by the HUD toggle gesture. Hiding and showing go through
// the game's own methods, and a HUD the game itself hides is never shown:
// the mod shows an object again only when it was the mod that hid it and
// the game is not hiding it for its own purpose.
namespace omvr::core::ui {

class HudVisibility final {
public:
    // What one update does.
    struct Step {
        std::array<HudAction, kHudMovieCount> actions{};
        bool toggled = false; // the gesture flipped the state in this update
        bool hidden  = false; // the player's state after this update
    };

    // At attach, before the game's threads run: the session's start state.
    void start(bool hidden) noexcept;

    // The game's hide and show passed their checks and are in place (22
    // §8.3: otherwise the toggle is off). Set on the game thread, read on
    // the render thread.
    void set_available(bool available) noexcept { available_.store(available, std::memory_order_release); }
    [[nodiscard]] bool available() const noexcept { return available_.load(std::memory_order_acquire); }

    // The HUD toggle gesture fired. Render thread (any thread).
    void request_toggle() noexcept { toggles_.fetch_add(1, std::memory_order_acq_rel); }

    // The game's own HUD entity hid or showed the HUD (22 §2.4). Game
    // thread.
    void on_game_hide() noexcept { game_hidden_.fill(true); }
    void on_game_show() noexcept { game_hidden_.fill(false); }

    // Nothing to do: the player's state is visible, nothing the mod hid is
    // left, and neither the setting nor the gesture asks for a change, so
    // the update need not look up the objects. Game thread.
    [[nodiscard]] bool idle(bool hidden_setting) const noexcept;

    // One game update: `hidden_setting` is Hud.Hidden now, `movies` what
    // the game's objects are now. Game thread.
    Step update(bool hidden_setting, const HudMovieStates& movies) noexcept;

private:
    std::atomic<bool> available_{false};
    std::atomic<std::uint32_t> toggles_{0};
    // Game thread.
    bool hidden_       = false;
    bool last_setting_ = false;
    std::array<bool, kHudMovieCount> mod_hid_{};     // the mod hid it, and nobody showed it since
    std::array<bool, kHudMovieCount> game_hidden_{}; // the game's own entity hides it
};

} // namespace omvr::core::ui
