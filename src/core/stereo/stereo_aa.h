#pragma once

#include <cstdint>

namespace omvr::core::stereo {

class StereoDevice;

// Anti-aliasing in stereo (not in the original).
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
// without projection jitter. Render thread.
class StereoAa {
public:
    explicit StereoAa(StereoDevice& stereo) noexcept : stereo_(stereo) {}

    // Replaces the full-screen draw of the stereo output path.
    void final_draw() noexcept;
    // Replaces the AA pass's call of the SMAA runner.
    void run_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept;

private:
    // The first stereo frame with each AA mode, and each change of what the
    // mod does with it: run it, run T2x as T1x, or skip it (setting off, or
    // Auto above its pixel scale). Once per change only.
    void log_stereo_aa(std::int32_t mode, bool run) noexcept;

    StereoDevice& stereo_;
    std::int32_t  s_mode = -1;
    int           s_action = -1;
};

} // namespace omvr::core::stereo
