#pragma once

namespace omvr::core::stereo {

// The "head in wall" blackout threshold the game compares against (05 §7.2):
// the patch points the game's comparison at this float, so it lives in
// static storage for the whole run. kHeadClipOn with clipping blinders on,
// kHeadClipOff (never black) with them off. Game thread.
float& head_clip_threshold() noexcept;
void   set_clipping_blinders(bool on) noexcept;

} // namespace omvr::core::stereo
