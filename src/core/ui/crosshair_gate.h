// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"

// Hud.Crosshair (spec 23 §7.1) around the game's CrosshairUpdate, which
// shows or hides the crosshair only when its decision differs from the
// cached one (+0xE4). While the crosshair is to be hidden the game's
// ShowCrosshairs is skipped (its ShowAmmo still runs), a crosshair already
// showing is hidden within one update, and once the setting allows it
// again the cache is cleared so that the next update in which the game
// wants the crosshair shows it. Game thread (the HUD update).
namespace omvr::core::ui {

// Whether the crosshair is to be hidden now: Off always, Auto while the
// weapon aims from the hand (21 §5).
[[nodiscard]] constexpr bool crosshair_hidden(config::CrosshairMode mode, bool hand_aiming) noexcept {
    return mode == config::CrosshairMode::Off || (mode == config::CrosshairMode::Auto && hand_aiming);
}

class CrosshairGate final {
public:
    // What to do before the game's update runs.
    struct Before {
        bool hide_now    = false; // invoke the movie's HideCrosshairs: it is showing
        bool clear_cache = false; // set the cached decision to "hidden"
    };

    // `hide`: crosshair_hidden now; `cached_shown`: the cached decision.
    Before before_update(bool hide, bool cached_shown) noexcept;

    // The game is about to invoke ShowCrosshairs: whether it may.
    bool allow_show(bool hide) noexcept;

    // The game is about to invoke HideCrosshairs: whether it needs to (not
    // when the mod already hid the crosshair, which would replay its out
    // animation).
    bool allow_hide() noexcept;

    // The movie shows no crosshair although the cache says it does.
    [[nodiscard]] bool hidden_by_mod() const noexcept { return hidden_by_mod_; }

private:
    bool hidden_by_mod_ = false;
};

} // namespace omvr::core::ui
