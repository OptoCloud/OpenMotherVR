// OpenMotherVR-Provenance: clean
#pragma once

#include <array>

// The lookup of the game's particle (alpha) lighting atlas in stereo
// (dumps/13_particle_lighting.md). The atlas tiles are laid out in the NDC
// of one camera per frame, the centre; the particle shaders address them
// with `ndc / scale - offset` of the eye they draw, where (offset, scale)
// is what the stereo device's slot 14 returns for that eye. That is the
// centre's NDC only when the centre's scale is 1, so in stereo the lookup
// is given the eye's scale and offset relative to the centre's.
namespace omvr::core::render {

// An NDC offset and scale in the game's float[4] order (device slot 14).
struct NdcScaleOffset {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale_x  = 1.0f;
    float scale_y  = 1.0f;
};

// The values to give the game's lookup for an eye whose slot 14 answer is
// `eye`, when the atlas was laid out with the centre's `centre`: chosen so
// that `ndc_eye / result.scale - result.offset` is the centre's NDC of the
// same view direction. `eye` itself when a scale is 0 or not finite.
NdcScaleOffset eye_ndc_in_centre(const NdcScaleOffset& eye, const NdcScaleOffset& centre) noexcept;

// Dev. Debug Options: whether the game's answer for an eye differs from the
// last one reported for that eye, so that the correction is logged once per
// change. Eye 1 is kept apart from every other eye. Render thread only.
class AlphaLightTrace final {
public:
    bool changed(int eye, const NdcScaleOffset& game) noexcept;

private:
    struct Last {
        NdcScaleOffset value{};
        bool any = false;
    };
    std::array<Last, 2> last_{};
};

} // namespace omvr::core::render
