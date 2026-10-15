#pragma once

namespace omvr::core::stereo {

// The "head in wall" blackout threshold the game compares against (its
// address is patched into the game, so it lives in static storage). 1e-5
// with clipping blinders on, 1.0 off.
float& head_clip_threshold() noexcept;
void   set_clipping_blinders(bool on) noexcept;

} // namespace omvr::core::stereo
