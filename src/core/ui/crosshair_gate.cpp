// OpenMotherVR-Provenance: clean
#include "core/ui/crosshair_gate.h"

namespace omvr::core::ui {

CrosshairGate::Before CrosshairGate::before_update(bool hide, bool cached_shown) noexcept {
    Before b;
    if (hide && cached_shown && !hidden_by_mod_) {
        // Showing when the setting changed: hidden within this update.
        b.hide_now     = true;
        hidden_by_mod_ = true;
    } else if (!hide && hidden_by_mod_) {
        // The cache says shown, the movie shows nothing: the game's next
        // "shown" decision differs from a cleared cache and shows it.
        b.clear_cache  = true;
        hidden_by_mod_ = false;
    }
    return b;
}

bool CrosshairGate::allow_show(bool hide) noexcept {
    if (!hide) return true;
    hidden_by_mod_ = true;
    return false;
}

bool CrosshairGate::allow_hide() noexcept {
    if (!hidden_by_mod_) return true;
    // The game now decides "hidden", which the movie already is.
    hidden_by_mod_ = false;
    return false;
}

} // namespace omvr::core::ui
