// The help text and glyph sets of the original's two schemes (OpenVR PollInput,
// FUN_10007ad0, and the Oculus backend, FUN_10004960); analysis
// dumps/06d_vr_input.md.

#include "core/input/control_scheme.h"

#include "core/input/glyph_ids.h"

namespace omvr::core::input {

struct ControlSchemes::HelpSet {
    std::span<const char* const> lines;
};

namespace {

// The original's help text for its two schemes (OpenVR and Oculus backends).
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

void set_glyph_scheme(Glyphs& glyphs, bool touchpad) noexcept {
    if (touchpad) {
        glyphs.set_glyph(0, kGlyphTouchpadDown);     // A: interact
        glyphs.set_glyph(1, kGlyphTouchpadCentre);   // B: radial
        glyphs.set_glyph(2, kGlyphTouchpadUp);       // X: reload
        glyphs.set_glyph(3, "Y");
        glyphs.set_glyph(8, kGlyphMenu);             // BACK: map
    } else {
        glyphs.set_glyph(0, kGlyphA);
        glyphs.set_glyph(1, kGlyphRightStick);
        glyphs.set_glyph(2, kGlyphB);
        glyphs.set_glyph(3, kGlyphY);                // Y: flashlight
        glyphs.set_glyph(8, kGlyphX);
    }
    glyphs.set_glyph(9, kGlyphMenu);                 // START: pause
    glyphs.set_glyph(10, kGlyphLeftGrip);            // LB: motion tracker
    glyphs.set_glyph(11, kGlyphRightGrip);           // RB: aim
}

} // namespace

ControlSchemes::ControlSchemes(Glyphs& glyphs) noexcept : glyphs_(glyphs), g_help{&kTouchpadSet} {}

void ControlSchemes::set(ControlScheme scheme) noexcept {
    set_glyph_scheme(glyphs_, scheme == ControlScheme::Touchpad);
    g_help.store(scheme == ControlScheme::Touchpad ? &kTouchpadSet
               : scheme == ControlScheme::Index    ? &kIndexSet
                                                   : &kThumbstickSet,
                 std::memory_order_release);
}

std::span<const char* const> ControlSchemes::control_help() const noexcept {
    return g_help.load(std::memory_order_acquire)->lines;
}

} // namespace omvr::core::input
