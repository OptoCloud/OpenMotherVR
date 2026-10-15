// OpenMotherVR-Provenance: clean
#include "core/stereo/head_clip_delay.h"

#include "core/stereo/tuning.h"
#include "core/stereo/types.h"

namespace omvr::core::stereo {

float HeadClipDelay::apply(float fade, bool stereo_running) noexcept {
    if (!stereo_running) return fade;
    const auto now = clock_.now();
    if (fade >= kFadeClear) {
        clear_at_ = now;
        return fade;
    }
    return now - clear_at_ < kFadeGrace ? kSceneVisible : fade;
}

} // namespace omvr::core::stereo
