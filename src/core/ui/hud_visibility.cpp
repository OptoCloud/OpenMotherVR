// OpenMotherVR-Provenance: clean
#include "core/ui/hud_visibility.h"

#include <cstddef>

namespace omvr::core::ui {

void HudVisibility::start(bool hidden) noexcept {
    hidden_       = hidden;
    last_setting_ = hidden;
}

bool HudVisibility::idle(bool hidden_setting) const noexcept {
    return !hidden_ && !mod_hid_[0] && !mod_hid_[1] && hidden_setting == last_setting_ &&
           toggles_.load(std::memory_order_acquire) == 0;
}

HudVisibility::Step HudVisibility::update(bool hidden_setting, const HudMovieStates& movies) noexcept {
    Step step;
    // 23 §7.2: a change on the options page sets the state at once.
    if (hidden_setting != last_setting_) {
        last_setting_ = hidden_setting;
        hidden_       = hidden_setting;
    }
    const std::uint32_t toggles = toggles_.exchange(0, std::memory_order_acq_rel);
    if (toggles % 2 != 0) hidden_ = !hidden_;
    step.toggled = toggles != 0;
    step.hidden  = hidden_;

    for (std::size_t i = 0; i < kHudMovieCount; ++i) {
        const HudMovieState& m = movies[i];
        if (!m.exists) continue; // only when the object exists (22 §8.3)
        if (!m.forced_hidden) {
            // Shown: by the game, or never hidden. Whatever hid it is over.
            mod_hid_[i]     = false;
            game_hidden_[i] = false;
            if (hidden_) {
                // 22 §8.3: re-applied on the next update that finds it shown.
                step.actions[i] = HudAction::Hide;
                mod_hid_[i]     = true;
            }
            continue;
        }
        if (hidden_ || !mod_hid_[i]) continue;
        // The player shows the HUD the mod hid; the game may be hiding it
        // for its own purpose meanwhile, and then it keeps it.
        if (!game_hidden_[i]) step.actions[i] = HudAction::Show;
        mod_hid_[i] = false;
    }
    return step;
}

} // namespace omvr::core::ui
