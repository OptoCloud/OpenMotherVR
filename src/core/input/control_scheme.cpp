// The help text and glyph sets of the original's two schemes (OpenVR PollInput,
// FUN_10007ad0, and the Oculus backend, FUN_10004960); analysis
// dumps/06d_vr_input.md. The rows are those of 04 §5.7; the per-controller
// glyphs and rows are 22 §10.

#include "core/input/control_scheme.h"

#include "core/input/glyph_ids.h"
#include "game/constants.h"

namespace omvr::core::input {

struct ControlSchemes::HelpSet {
    std::span<const char* const> lines;
    // The HUD toggle's row: its control is the set's Map Menu control
    // (22 §10.3).
    const char* hud_row;
    // The wand's control texts (`L` / `R`), else the thumbstick style.
    bool wand_style;
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

// The Steam Frame (04 §5.7): Flashlight on the left bumper, Map on the
// View button, Pause on the right Menu button, and a d-pad for menus.
constexpr const char* kFrameHelp[] = {
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
    "Flashlight\tLeft Bumper",
    "Flashlight [Press Interact near Head]\tA",
    "Next Weapon\tRight Thumbstick Up",
    "Previous Weapon\tRight Thumbstick Down",
    "Map Menu\tView Button",
    "Pause Menu\tMenu Button",
    "Menu Navigation\tLeft D-Pad",
};

// With Controls.ControllerGlyphs (22 §10.3): Index names the buttons its
// controllers are marked with.
constexpr const char* kIndexFamilyHelp[] = {
    "Move\tLeft Thumbstick",
    "Sprint\tLeft Thumbstick Click",
    "Crouch\tLeft Trigger",
    "Turn\tRight Thumbstick L/R",
    "Aim\tRight Grip",
    "Fire / Throw / Melee\tRight Trigger",
    "Motion Tracker\tLeft Grip",
    "Interact\tRight A",
    "Radial Menu\tRight Thumbstick Click",
    "Reload / Activate\tRight B",
    "Flashlight\tLeft B",
    "Flashlight [Press Interact near Head]\tRight A",
    "Next Weapon\tRight Thumbstick Up",
    "Previous Weapon\tRight Thumbstick Down",
    "Map Menu\tLeft A",
    "Pause Menu\tLeft Trackpad Press",
};
// WMR under SteamVR Input: touchpad halves for the face-button roles.
constexpr const char* kWmrSteamVrHelp[] = {
    "Move\tLeft Thumbstick",
    "Sprint\tLeft Thumbstick Click",
    "Crouch\tLeft Trigger",
    "Turn\tRight Thumbstick L/R",
    "Aim\tRight Grip",
    "Fire / Throw / Melee\tRight Trigger",
    "Motion Tracker\tLeft Grip",
    "Interact\tRight Touchpad Down",
    "Radial Menu\tRight Thumbstick Click",
    "Reload / Activate\tRight Touchpad Up",
    "Flashlight\tLeft Touchpad Up",
    "Flashlight [Press Interact near Head]\tRight Touchpad Down",
    "Next Weapon\tRight Thumbstick Up",
    "Previous Weapon\tRight Thumbstick Down",
    "Map Menu\tLeft Touchpad Down",
    "Pause Menu\tMenu Button",
};
// WMR under OpenXR: one click per touchpad, no reload or flashlight control.
constexpr const char* kWmrOpenXrHelp[] = {
    "Move\tLeft Thumbstick",
    "Sprint\tLeft Thumbstick Click",
    "Crouch\tLeft Trigger",
    "Turn\tRight Thumbstick L/R",
    "Aim\tRight Grip",
    "Fire / Throw / Melee\tRight Trigger",
    "Motion Tracker\tLeft Grip",
    "Interact\tRight Touchpad Click",
    "Radial Menu\tRight Thumbstick Click",
    "Flashlight [Press Interact near Head]\tRight Touchpad Click",
    "Next Weapon\tRight Thumbstick Up",
    "Previous Weapon\tRight Thumbstick Down",
    "Map Menu\tLeft Touchpad Click",
    "Pause Menu\tMenu Button",
};

// The HUD toggle row of each set: its control is the set's Map Menu control.
constexpr const char* kHudRowWand        = "Hide/Show HUD [Left Hand near Head]\tL Menu";
constexpr const char* kHudRowThumbstick  = "Hide/Show HUD [Left Hand near Head]\tX";
constexpr const char* kHudRowFrame       = "Hide/Show HUD [Left Hand near Head]\tView Button";
constexpr const char* kHudRowIndexFamily = "Hide/Show HUD [Left Hand near Head]\tLeft A";
constexpr const char* kHudRowWmrSteamVr  = "Hide/Show HUD [Left Hand near Head]\tLeft Touchpad Down";
constexpr const char* kHudRowWmrOpenXr   = "Hide/Show HUD [Left Hand near Head]\tLeft Touchpad Click";

using HelpSet = ControlSchemes::HelpSet;
const HelpSet kTouchpadSet{kTouchpadHelp, kHudRowWand, true};
const HelpSet kThumbstickSet{kThumbstickHelp, kHudRowThumbstick, false};
const HelpSet kIndexSet{kIndexHelp, kHudRowThumbstick, false};
const HelpSet kFrameSet{kFrameHelp, kHudRowFrame, false};
const HelpSet kIndexFamilySet{kIndexFamilyHelp, kHudRowIndexFamily, false};
const HelpSet kWmrSteamVrSet{kWmrSteamVrHelp, kHudRowWmrSteamVr, false};
const HelpSet kWmrOpenXrSet{kWmrOpenXrHelp, kHudRowWmrOpenXr, false};

// The rows of v2 features (22 §10.3), each while its key is on, in this
// order; thumbstick style, then the wand's.
constexpr const char* kDpadRow       = "D-Pad\tHold Left Thumbstick Click + Left Thumbstick";
constexpr const char* kDpadRowWand   = "D-Pad\tL Touchpad Edge Click";
constexpr const char* kRecenterRow   = "Recenter [Both Hands near Head]\tHold Both Grips";
constexpr const char* kHolsterRows[] = {
    "Next Weapon [Grip over Right Shoulder]\tRight Grip",
    "Previous Weapon [Grip over Left Shoulder]\tLeft Grip",
    "Flashlight [Grip at Left Hip]\tLeft Grip",
    "Motion Tracker [Grip at Chest]\tLeft or Right Grip",
};
constexpr const char* kHolsterRowsWand[] = {
    "Next Weapon [Grip over Right Shoulder]\tR Grip",
    "Previous Weapon [Grip over Left Shoulder]\tL Grip",
    "Flashlight [Grip at Left Hip]\tL Grip",
    "Motion Tracker [Grip at Chest]\tL or R Grip",
};
constexpr const char* kHidingLeanRow = "Lean [While Hiding]\tLean Your Head";
constexpr const char* kHoldBreathRow = "Hold Breath [While Hiding]\tHand over Mouth";

const HelpSet& help_set(ControlScheme scheme) noexcept {
    switch (scheme) {
        case ControlScheme::Touchpad: return kTouchpadSet;
        case ControlScheme::Index: return kIndexSet;
        case ControlScheme::Frame: return kFrameSet;
        case ControlScheme::Thumbstick: break;
    }
    return kThumbstickSet;
}

const HelpSet& family_help_set(GlyphFamily family, ControllerSource source) noexcept {
    if (source == ControllerSource::OpenVrLegacy) return kTouchpadSet;
    switch (family) {
        case GlyphFamily::Index: return kIndexFamilySet;
        case GlyphFamily::Wmr: return source == ControllerSource::OpenXr ? kWmrOpenXrSet : kWmrSteamVrSet;
        case GlyphFamily::Wand: return kTouchpadSet;
        case GlyphFamily::Frame: return kFrameSet;
        case GlyphFamily::Touch:
        case GlyphFamily::Generic:
        case GlyphFamily::Unknown: break;
    }
    return kThumbstickSet;
}

const char* source_name(ControllerSource source) noexcept {
    switch (source) {
        case ControllerSource::SteamVrInput: return "SteamVR Input";
        case ControllerSource::OpenVrLegacy: return "OpenVR legacy input";
        case ControllerSource::OpenXr: return "OpenXR";
    }
    return "SteamVR Input";
}

// The controls the tests construct with: every v2 key at its v1 value.
constexpr config::ControlsSettings kV1Controls{};

} // namespace

ControlSchemes::ControlSchemes(Glyphs& glyphs) noexcept
    : glyphs_(glyphs), controls_(kV1Controls), log_(nullptr), help_{&kTouchpadSet} {}

ControlSchemes::ControlSchemes(Glyphs& glyphs, const config::ControlsSettings& controls,
                               diag::ports::Log& log) noexcept
    : glyphs_(glyphs), controls_(controls), log_(&log), help_{&kTouchpadSet} {}

void ControlSchemes::set(ControlScheme scheme) noexcept {
    scheme_        = scheme;
    scheme_chosen_ = true;
    apply();
}

void ControlSchemes::identify(std::size_t hand, ControllerSource source, std::string_view id) noexcept {
    if (hand >= families_.size()) return;
    const bool on            = controls_.controller_glyphs;
    const GlyphFamily family = glyph_family(source, id);
    const bool changed       = family != families_[hand] || source != source_ || on != families_on_;
    families_[hand]          = family;
    source_                  = source;
    families_on_             = on;
    if (!changed) return;
    apply();
    if (on && log_)
        diag::ports::logf(*log_, "[input] controller glyphs: left {}, right {} ({})",
                          family_name(families_[0]), family_name(families_[1]), source_name(source_));
}

void ControlSchemes::refresh() noexcept {
    if (controls_.dpad_emulation == dpad_on_) return;
    apply();
}

const ControlSchemes::HelpSet& ControlSchemes::v2_help_set() const noexcept {
    // The page holds one list: it follows the right hand's family (22 §10.3).
    const GlyphFamily f = families_[1] != GlyphFamily::Unknown ? families_[1] : families_[0];
    return family_help_set(f, source_);
}

void ControlSchemes::apply() noexcept {
    dpad_on_ = controls_.dpad_emulation;
    const bool family =
        families_on_ && (families_[0] != GlyphFamily::Unknown || families_[1] != GlyphFamily::Unknown ||
                         source_ == ControllerSource::OpenVrLegacy);
    if (!family && !scheme_chosen_) return; // the game's own names until a set is chosen (04 §4.1)
    bool changed = false;
    for (int code = 0; code < static_cast<int>(glyphs_.size()); ++code) {
        const char* name =
            family ? family_glyph(families_[0], families_[1], source_, code, dpad_on_) : nullptr;
        changed |= glyphs_.set_glyph(code, name ? name : scheme_glyph(scheme_, code, dpad_on_));
    }
    // A v2 change redraws the prompts on screen without the player touching
    // another device (22 §10.1); v1's one choice per run needs no redraw.
    const bool v2 = family || dpad_on_;
    if (changed && (v2 || v2_applied_)) glyphs_.request_redraw();
    v2_applied_ = v2;
    // Release pairs with the acquire in control_help: a reader that gets the
    // new set also sees the set's entries.
    help_.store(family ? &v2_help_set() : &help_set(scheme_), std::memory_order_release);
}

HelpRows ControlSchemes::control_help() const noexcept {
    const HelpSet& set = *help_.load(std::memory_order_acquire);
    HelpRows rows;
    rows.append(set.lines);
    // The rows of v2 features, whatever Controls.ControllerGlyphs says.
    if (controls_.dpad_emulation) rows.push_back(set.wand_style ? kDpadRowWand : kDpadRow);
    if (controls_.recenter_gesture) rows.push_back(kRecenterRow);
    if (controls_.hud_toggle_gesture) rows.push_back(set.hud_row);
    if (controls_.holster_gestures) rows.append(set.wand_style ? kHolsterRowsWand : kHolsterRows);
    if (controls_.hiding_lean) rows.push_back(kHidingLeanRow);
    if (controls_.hold_breath_gesture) rows.push_back(kHoldBreathRow);
    return rows;
}

} // namespace omvr::core::input
