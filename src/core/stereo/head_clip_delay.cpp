#include "core/stereo/head_clip_delay.h"

#include "core/stereo/tuning.h"

namespace omvr::core::stereo {

float HeadClipDelay::apply(float fade, bool stereo_running) noexcept {
    if (!stereo_running) return fade;
    const auto now = clock_.now();
    if (fade >= kFadeClear) {
        g_head_clear_at = now;
        return fade;
    }
    return now - g_head_clear_at < kFadeGrace ? 1.0f : fade;
}

} // namespace omvr::core::stereo
