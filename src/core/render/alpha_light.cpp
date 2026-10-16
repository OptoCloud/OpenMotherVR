// OpenMotherVR-Provenance: clean
#include "core/render/alpha_light.h"

#include <cmath>

namespace omvr::core::render {

namespace {

bool usable(float scale) noexcept { return std::isfinite(scale) && scale != 0.0f; }

bool same(const NdcScaleOffset& a, const NdcScaleOffset& b) noexcept {
    return a.offset_x == b.offset_x && a.offset_y == b.offset_y && a.scale_x == b.scale_x &&
           a.scale_y == b.scale_y;
}

} // namespace

// Per axis, with the eye's NDC n_e = s_e * t + o_e and the centre's
// n_c = s_c * t + o_c of a view tangent t: n_c = n_e * (s_c / s_e) -
// (s_c * o_e / s_e - o_c), which is the game's n_e / S - O for
// S = s_e / s_c and O = s_c * o_e / s_e - o_c.
NdcScaleOffset eye_ndc_in_centre(const NdcScaleOffset& eye, const NdcScaleOffset& centre) noexcept {
    if (!usable(eye.scale_x) || !usable(eye.scale_y) || !usable(centre.scale_x) || !usable(centre.scale_y)) {
        return eye;
    }
    return {.offset_x = centre.scale_x * eye.offset_x / eye.scale_x - centre.offset_x,
            .offset_y = centre.scale_y * eye.offset_y / eye.scale_y - centre.offset_y,
            .scale_x  = eye.scale_x / centre.scale_x,
            .scale_y  = eye.scale_y / centre.scale_y};
}

bool AlphaLightTrace::changed(int eye, const NdcScaleOffset& game) noexcept {
    Last& last = last_[eye == 1 ? 1 : 0];
    if (last.any && same(game, last.value)) return false;
    last = {.value = game, .any = true};
    return true;
}

} // namespace omvr::core::render
