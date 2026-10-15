#pragma once

#include "core/config/settings.h"
#include "core/stereo/types.h"
#include "core/vr/types.h"

#include <array>

namespace omvr::core::stereo {

// What the desktop mirror's layout depends on, read from the stereo device.
struct MirrorView {
    PixelSize texture;                           // the side-by-side stereo texture
    PixelSize window;                            // the desktop window, zero while unknown
    std::array<vr::FovTangents, 2> real_fov;     // each eye's real FOV (DefaultEyeFov)
    std::array<vr::FovTangents, 2> rendered_fov; // the FOV each eye was rendered with (MaxEyeFov)
    config::MirrorViewType type = config::MirrorViewType::RightEye;
    bool symmetric              = true;
};

// What the window shows (03 §8), for the current layout, window size and
// mirror settings. Head-tracked frames (`screen` false) show the eye
// image(s), filling the window width at their own aspect (a near-square eye
// in a 16:9 window is cropped top and bottom). Screen frames show the region
// the headset's virtual screen shows, kScreenMirrorSource, fitted whole.
// Outside the draw area the shader paints black. Pure.
MirrorConstants mirror_constants(const MirrorView& view, bool screen) noexcept;

} // namespace omvr::core::stereo
