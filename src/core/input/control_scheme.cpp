// The help text and glyph sets of the original's two schemes (OpenVR PollInput,
// FUN_10007ad0, and the Oculus backend, FUN_10004960); analysis
// dumps/06d_vr_input.md. The rows are those of 04 §5.7.

#include "core/input/control_scheme.h"

#include "game/constants.h"
#include "core/input/glyph_ids.h"

namespace omvr::core::input {

struct ControlSchemes::HelpSet {
    std::span<const char* const> lines;
};

namespace {

// The controls help rows, function and control separated by a tab (04 §5.7,
// exact text).
constexpr const char* kTouchpadHelp[] = {
    "Move\tL Touchpad",
    "Sprint\tL Touchpad Click",
    "Crouch\tL Trigger",
    "Turn\tR Touchpad L/R",
    "Aim\tR Grip",
    "Fire / Throw / Melee\tR Trigger",
    "Motion Tracker\tL Grip",
    "Interact\tR Touchpad Down",
    "Radial Menu\tR Touchpad Center",
    "Reload / Activate\tR Touchpad Up",
    "Flashlight [Press Interact near Head]\tR Touchpad Down",
    "Map Menu\tL Menu",
    "Pause Menu\tR Menu",
};
constexpr const char* kThumbstickHelp[] = {
    "Move\tLeft Thumbstick",
    "Sprint\tLeft Thumbstick Click",
    "Crouch\tLeft Trigger",
    "Turn\tRight Thumbstick L/R",
    "Aim\tRight Grip",
    "Fire / Throw / Melee\tRight Trigger",
    "Motion Tracker\tLeft Grip",
    "Interact\tA",
    "Radial Menu\tRight Thumbstick Click",
    "Reload / Activate\tB",
    "Flashlight\tY",
    "Flashlight [Press Interact near Head]\tA",
    "Next Weapon\tRight Thumbstick Up",
    "Previous Weapon\tRight Thumbstick Down",
    "Map Menu\tX",
    "Pause Menu\tMenu Button",
};
// Index has no menu button: the left trackpad press pauses.
constexpr const char* kIndexHelp[] = {
    "Move\tLeft Thumbstick",
    "Sprint\tLeft Thumbstick Click",
    "Crouch\tLeft Trigger",
    "Turn\tRight Thumbstick L/R",
    "Aim\tRight Grip",
    "Fire / Throw / Melee\tRight Trigger",
    "Motion Tracker\tLeft Grip",
    "Interact\tA",
    "Radial Menu\tRight Thumbstick Click",
    "Reload / Activate\tB",
    "Flashlight\tY",
    "Flashlight [Press Interact near Head]\tA",
    "Next Weapon\tRight Thumbstick Up",
    "Previous Weapon\tRight Thumbstick Down",
    "Map Menu\tX",
    "Pause Menu\tLeft Trackpad Press",
};

using HelpSet = ControlSchemes::HelpSet;
const HelpSet kTouchpadSet{kTouchpadHelp};
const HelpSet kThumbstickSet{kThumbstickHelp};
const HelpSet kIndexSet{kIndexHelp};

// The glyph sets of 04 §4.1, keyed by the stock pad control each game
// function is bound to (04 §2.4: A interact, B radial, X reload, Y
// flashlight, BACK map, START pause, LB tracker, RB aim).
void set_touchpad_glyphs(Glyphs& glyphs) noexcept {
    glyphs.set_glyph(game::kPadA, kGlyphTouchpadDown);       // interact
    glyphs.set_glyph(game::kPadB, kGlyphTouchpadCentre);     // radial
    glyphs.set_glyph(game::kPadX, kGlyphTouchpadUp);         // reload
    glyphs.set_glyph(game::kPadY, kStockPadNames[game::kPadY]);    // no flashlight button: the stock name
    glyphs.set_glyph(game::kPadBack, kGlyphMenu);            // map
}

void set_thumbstick_glyphs(Glyphs& glyphs) noexcept {
    glyphs.set_glyph(game::kPadA, kGlyphA);
    glyphs.set_glyph(game::kPadB, kGlyphRightStick);
    glyphs.set_glyph(game::kPadX, kGlyphB);
    glyphs.set_glyph(game::kPadY, kGlyphY);                  // flashlight
    glyphs.set_glyph(game::kPadBack, kGlyphX);
}

void set_glyph_scheme(Glyphs& glyphs, bool touchpad) noexcept {
    if (touchpad) {
        set_touchpad_glyphs(glyphs);
    } else {
        set_thumbstick_glyphs(glyphs);
    }
    glyphs.set_glyph(game::kPadStart, kGlyphMenu);           // pause
    glyphs.set_glyph(game::kPadLB, kGlyphLeftGrip);          // motion tracker
    glyphs.set_glyph(game::kPadRB, kGlyphRightGrip);         // aim
}

const HelpSet& help_set(ControlScheme scheme) noexcept {
    switch (scheme) {
        case ControlScheme::Touchpad: return kTouchpadSet;
        case ControlScheme::Index: return kIndexSet;
        case ControlScheme::Thumbstick: break;
    }
    return kThumbstickSet;
}

} // namespace

ControlSchemes::ControlSchemes(Glyphs& glyphs) noexcept : glyphs_(glyphs), help_{&kTouchpadSet} {}

void ControlSchemes::set(ControlScheme scheme) noexcept {
    set_glyph_scheme(glyphs_, scheme == ControlScheme::Touchpad);
    // Release pairs with the acquire in control_help: a reader that gets the
    // new set also sees the set's entries.
    help_.store(&help_set(scheme), std::memory_order_release);
}

std::span<const char* const> ControlSchemes::control_help() const noexcept {
    return help_.load(std::memory_order_acquire)->lines;
}

} // namespace omvr::core::input
