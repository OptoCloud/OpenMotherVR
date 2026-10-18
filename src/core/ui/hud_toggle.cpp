// OpenMotherVR-Provenance: clean
#include "core/ui/hud_toggle.h"

#include "core/input/tuning.h"

namespace omvr::core::ui {

namespace {

constexpr std::string_view kLogFired = "hud: toggle gesture fired";
constexpr std::string_view kLogSuppressedFocus =
    "[diag] hud: toggle gesture suppressed: no input focus, or just after it returned";

} // namespace

HudToggleGesture::Result HudToggleGesture::update(bool enabled, const input::Controls& c,
                                                  TimePoint now) noexcept {
    Result r;
    const bool open   = gate_.open(c.poses.focused, now);
    const bool rising = c.map && !prev_map_;
    prev_map_         = c.map;
    if (!c.map) {
        owner_ = Owner::None;
        return r;
    }
    if (rising) {
        const bool near = enabled && input::hand_near_head(c.poses, input::kLeftHand, input::kNearHead);
        owner_          = near ? Owner::Toggle : Owner::Map;
        if (near) {
            r.fired            = open;
            r.suppressed_focus = !open;
        }
    }
    r.take_map = owner_ == Owner::Toggle;
    return r;
}

void HudToggleSink::write(const input::Controls& c, bool right_hand_near_head) noexcept {
    const bool enabled = deps_.controls.hud_toggle_gesture && deps_.visibility.available();
    const auto r       = gesture_.update(enabled, c, deps_.clock.now());
    if (r.fired) {
        deps_.visibility.request_toggle();
        deps_.log.line(kLogFired);
    } else if (r.suppressed_focus && deps_.log.debug_enabled()) {
        deps_.log.line(kLogSuppressedFocus);
    }
    if (!r.take_map) {
        deps_.next.write(c, right_hand_near_head);
        return;
    }
    input::Controls taken = c;
    taken.map             = false;
    deps_.next.write(taken, right_hand_near_head);
}

} // namespace omvr::core::ui
