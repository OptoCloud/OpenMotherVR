#pragma once

#include "core/stereo/types.h"
#include "game/layout.h" // R: stereo (A3): the layout from value structs, not the game's device

namespace omvr::core::stereo {

// What the window shows, for the current layout, window size and mirror
// settings. Head-tracked frames (`screen` false) show the eye image(s),
// filling the window width at their own aspect (a near-square eye in a
// 16:9 window is cropped top and bottom). Screen frames show the region the
// headset's virtual screen shows, eye 0's half, middle half vertically,
// fitted whole. Outside the draw area the shader paints black.
MirrorConstants mirror_constants(const game::StereoInterface& si, bool screen) noexcept;

} // namespace omvr::core::stereo
