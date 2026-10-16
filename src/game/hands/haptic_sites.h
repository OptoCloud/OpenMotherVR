// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"
#include "game/hands/hand_objects.h"
#include "game/patcher.h"

// The game sites of the haptics (spec 21 §14, §20), checked and patched
// only when Hands.Haptics.Enabled is on at start (21 §14.3: off, nothing is
// hooked).
namespace omvr::game::hands {

// RumbleSetMotors (21 §2.9): __thiscall on the XInput rumble object, the
// low- and high-frequency motor levels (0 to 1), ret 8; the result in EAX.
// Called as __fastcall with an unused EDX.
using RumbleSetMotorsFn = int(__fastcall*)(void* self, void* edx, float low, float high);

// The replacements the sites are pointed at.
struct HapticHooks {
    RumbleSetMotorsFn rumble_set_motors; // the game's rumble output
    const void* tracker_scan;            // bool __fastcall (tracker, edx): TrackerScan's ABI
};

// What the replacements call, set before each goes live.
struct HapticTargets {
    RumbleSetMotorsFn rumble_set_motors = nullptr; // the trampoline to the game's own
};

// Which events have their game source in place.
struct HapticSitesOutcome {
    bool rumble  = false; // the game's rumble (21 §2.9)
    bool player  = false; // the player's health, damage and death (21 §2.2)
    bool firing  = false; // the current weapon for the firing pulse (21 §2.5)
    bool tracker = false; // the tracker scan (21 §2.6)
};

// Checks each source before writing anything for it (12 §4); a source
// whose code differs, or whose addresses this build lacks, stays off and
// is logged. The player and CurrentItem go to `hand_out`, like the hands'.
HapticSitesOutcome install_haptic_sites(const GameImage& g, Patcher& p, const HapticHooks& hooks,
                                        HandTargets& hand_out, HapticTargets& out) noexcept;

} // namespace omvr::game::hands
