// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/stereo/types.h"
#include "util/sync/change_latch.h"

#include <utility>

namespace omvr::core::stereo {

// What the stereo output does with the game's AA mode.
enum class AaAction {
    Skip     = 0, // setting off, or Auto above its pixel scale
    Run      = 1, // run the AA pass as the game's mode says
    RunAsT1x = 2, // SMAA T2x runs as SMAA T1x
};

// Anti-aliasing in stereo (08 §3).
//
// In stereo the game's final post chain takes its own output path: it binds
// postprocess_stereo_final, sets the head-in-wall fade in PS c36 and draws
// straight into the output, never calling the AA pass. Mono runs the AA
// pass instead, which draws the bound final effect into an intermediate
// target and then runs FXAA or SMAA from it into the output. With stereo
// running and AA on, the stereo output draw is replaced by that AA pass,
// so the stereo final effect (fade included) feeds FXAA/SMAA exactly as the
// mono effect does.
//
// FXAA and SMAA 1x are spatial: they act on the side-by-side image, and the
// only pixels they can mix across the eye seam are the column on each side
// of it, at the outer edge of each eye's view. SMAA T2x is temporal: it
// needs per-eye velocities and per-eye previous view-projections, which the
// game keeps for one eye only (the velocity pass runs for eye 0), so its
// history would reproject the right eye with the left eye's motion. In
// stereo T2x therefore runs as SMAA 1x, without the temporal resolve and
// without projection jitter. Decisions only: the thunks call the game.
// Render thread.
class StereoAa final {
public:
    // `settings` is the live settings object: StereoAntiAliasing and the
    // pixel scale are read each frame.
    StereoAa(const config::Settings& settings, diag::ports::Log& log) noexcept
        : settings_(settings), log_(log) {}

    // The stereo output runs the game's AA pass for `mode` instead of its
    // own full-screen draw: an AA mode is set, and StereoAntiAliasing is on,
    // or Auto at a pixel scale where the eyes are not already supersampled
    // (above it the pass costs about 0.6 ms GPU for little gain).
    bool runs_aa_pass(AaMode mode) const noexcept;

    // The first stereo frame with each AA mode, and each change of what the
    // mod does with it (AaAction). Once per change only.
    void log_decision(AaMode mode, bool run) noexcept;

    // The AA pass's SMAA runs spatially (without the temporal resolve, which
    // also zeroes the next frame's projection jitter) while stereo runs.
    static bool spatial_smaa(bool temporal, bool stereo_running) noexcept {
        return temporal && stereo_running;
    }

private:
    const config::Settings& settings_;
    diag::ports::Log& log_;
    // The last logged mode and action; none before the first.
    util::ChangeLatch<std::pair<AaMode, AaAction>> logged_;
};

} // namespace omvr::core::stereo
