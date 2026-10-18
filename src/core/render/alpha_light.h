// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/tuning.h"
#include "util/math/matrix.h"
#include "util/math/vec.h"
#include "util/sync/frame_period.h"

#include <array>
#include <cstddef>

// The lookup of the game's particle (alpha) lighting atlas in stereo
// (08 §8). The atlas tiles are laid out in the NDC
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

// 08 §8.5: how far the drawn eye's camera is from the atlas camera the
// tiles were laid out with: the angle between their forward axes in degrees
// (NaN when an axis has no length) and the distance between their
// positions in metres. `eye_view` is the eye's world-to-view matrix (05 §1:
// row vectors, column 2 the forward axis).
struct CameraGap {
    float angle_deg  = 0.0f;
    float distance_m = 0.0f;
};
CameraGap atlas_camera_gap(const util::Vec3& atlas_forward, const util::Vec3& atlas_position,
                           const util::Matrix44& eye_view) noexcept;

// Dev. Debug Options traces of the lookup. Eye 1 is kept apart from every
// other eye. Render thread only.
class AlphaLightTrace final {
public:
    // Whether the game's answer for an eye differs from the last one
    // reported for that eye, so that the correction is logged once per change.
    bool changed(int eye, const NdcScaleOffset& game) noexcept;

    // Whether this lookup is the first of its eye in a frame that falls on
    // the diagnostic's period (08 §8.5: every 900 frames). A frame starts
    // when the lookup's eye goes back from eye 1 to the other one, so only
    // frames with lit particles in both eyes count.
    bool camera_sample_due(int eye) noexcept;

private:
    struct Last {
        NdcScaleOffset value{};
        bool any = false;
    };
    std::array<Last, 2> last_{};

    util::FramePeriod frames_{diag::kDebugLogPeriodFrames};
    std::size_t previous_slot_ = 0;
    bool sampling_             = false;
    std::array<bool, 2> sampled_{};
};

} // namespace omvr::core::render
