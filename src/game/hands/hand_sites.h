// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"
#include "game/hands/hand_objects.h"
#include "game/patcher.h"

// The game sites of the hand features (spec 21 §20), checked and patched
// only when Hands.Enabled is on (21 §1: at the defaults nothing is hooked).
namespace omvr::game::hands {

// The replacements the sites are pointed at.
struct HandHooks {
    const void* aim_ray_update; // void __fastcall (component, edx): AimRayUpdate's ABI
    const void* tracker_scan;   // bool __fastcall (tracker, edx): TrackerScan's ABI
    float* fire_gate_threshold; // the float the fire alignment test is pointed at
};

// Which features went in.
struct HandSitesOutcome {
    bool aim     = false; // the aim ray call and the fire alignment test (21 §5)
    bool tracker = false; // the tracker scan call (21 §6)
};

// Checks every site of a feature before writing any of it (12 §4); a
// feature whose code differs, or whose addresses this build lacks, stays
// off and is logged. The functions the replacements call go to `out`
// before the first redirect. `fire_gate_threshold` gets the game's own
// threshold, so pointing the test at it changes nothing until the hand
// aims.
HandSitesOutcome install_hand_sites(const GameImage& g, Patcher& p, const HandHooks& hooks,
                                    HandTargets& out) noexcept;

} // namespace omvr::game::hands
