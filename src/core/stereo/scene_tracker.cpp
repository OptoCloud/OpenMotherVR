#include "core/stereo/scene_tracker.h"

namespace omvr::core::stereo {

void SceneTracker::on_message(std::uint32_t id, const SceneMessages& names) noexcept {
    if (id == names.cutscene_start || id == names.fade_start) {
        cutscene_or_fade_ = true;
    } else if (id == names.cutscene_finish || id == names.fade_finish) {
        cutscene_or_fade_ = false;
    }
}

std::optional<float> SceneTracker::update(bool follow_active, StereoMode mode) noexcept {
    if (!cutscene_or_fade_) return std::nullopt;
    std::optional<float> visibility;
    if (mode != StereoMode::Screen) visibility = kSceneVisible;
    if (!follow_active) {
        took_camera_ = true;
    } else if (took_camera_) {
        cutscene_or_fade_ = took_camera_ = false;
    }
    return visibility;
}

} // namespace omvr::core::stereo
