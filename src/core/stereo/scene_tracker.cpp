#include "core/stereo/scene_tracker.h"

#include "game/addresses.h" // R: stereo (A3): the message hashes handed in as values

namespace omvr::core::stereo {

void SceneTracker::on_message(std::uint32_t id) noexcept {
    using game::AiAddr;
    auto name = [](AiAddr slot) { return *reinterpret_cast<const std::uint32_t*>(game::ai_address(slot)); };
    if (id == name(AiAddr::CutsceneVisibilityStart) || id == name(AiAddr::LevelFadeStart)) {
        g_cutscene_or_fade = true;
    } else if (id == name(AiAddr::CutsceneVisibilityFinish) || id == name(AiAddr::LevelFadeFinish)) {
        g_cutscene_or_fade = false;
    }
}

} // namespace omvr::core::stereo
