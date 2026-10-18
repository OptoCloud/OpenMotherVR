// OpenMotherVR-Provenance: clean
#include "core/input/dpad_emulation.h"

#include "core/input/touchpad.h"
#include "core/input/tuning.h"

#include <cmath>

namespace omvr::core::input {

namespace {

// How far `stick` reaches along the direction `d` holds (one at a time).
float along(Vec2 stick, Dpad d) noexcept {
    if (d.up) return stick.y;
    if (d.down) return -stick.y;
    if (d.left) return -stick.x;
    if (d.right) return stick.x;
    return 0.0f;
}

// The direction of the larger axis of `stick`, whatever its length.
Dpad larger_axis(Vec2 stick) noexcept {
    if (std::fabs(stick.x) > std::fabs(stick.y)) return {.left = stick.x < 0.0f, .right = stick.x > 0.0f};
    return {.up = stick.y > 0.0f, .down = stick.y < 0.0f};
}

} // namespace

Dpad stick_dpad(Vec2 stick, Dpad held) noexcept {
    if (held.any() && along(stick, held) >= kDpadRelease) return held;
    if (std::fmax(std::fabs(stick.x), std::fabs(stick.y)) <= kDpadPress) return {};
    return larger_axis(stick);
}

// 22 §7: the d-pad modifier is Sprint's control. On thumbsticks (the left
// stick click) it engages once held for kDpadModifierHold; on wands (the
// left pad click) a click on an edge engages at once, and one in the centre
// stays sprint.
bool DpadEmulation::modifier_engaged(const Controls& c, std::chrono::steady_clock::time_point now) noexcept {
    const bool pressed = c.sprint && (!c.touchpad_scheme || !decode_touchpad(c.move, true).centre);
    if (!pressed) {
        modifier_since_.reset();
        return false;
    }
    if (c.touchpad_scheme) return true;
    if (!modifier_since_) modifier_since_ = now;
    return now - *modifier_since_ >= kDpadModifierHold;
}

DpadEmulationResult DpadEmulation::update(const Controls& c, bool stick_free, bool enabled,
                                          std::chrono::steady_clock::time_point now) noexcept {
    if (!enabled) {
        held_ = {};
        modifier_since_.reset();
        return {};
    }
    DpadEmulationResult r;
    r.modifier_held = modifier_engaged(c, now);
    if (!stick_free && !r.modifier_held) {
        held_ = {};
        return r;
    }
    // A wand's edge click is the press itself: its direction is where the
    // pad was clicked.
    held_        = c.touchpad_scheme && r.modifier_held ? larger_axis(c.move) : stick_dpad(c.move, held_);
    r.dpad       = held_;
    r.stick_used = held_.any();
    return r;
}

} // namespace omvr::core::input
