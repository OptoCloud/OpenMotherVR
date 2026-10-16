#pragma once

#include "core/stereo/types.h"

#include <cstdint>
#include <optional>

namespace omvr::core::stereo {

// Whether a cutscene or level fade is running, and whether the camera left
// the player during it (05 §6.2, §3.2g). Original: DAT_1006863a,
// DAT_1006863b. Game thread.
class SceneTracker final {
public:
    // A UI entity message: cutscene and level-fade start/finish switch the
    // tracker; `id` is the message's name hash.
    // From: FUN_1001f830.
    void on_message(std::uint32_t id, const SceneMessages& names) noexcept;

    // Once per stereo update: the scene visibility to force for this frame
    // (never black during cutscenes and fades), or nullopt to leave the
    // game's. A cutscene ends for the tracker once the camera that left the
    // player is back.
    std::optional<float> update(bool follow_active, StereoMode mode) noexcept;

    // A cutscene or level fade is in progress (spec 22 §4.4 blocks turning).
    bool cutscene_or_fade() const noexcept { return cutscene_or_fade_; }

private:
    bool cutscene_or_fade_ = false;
    bool took_camera_      = false;
};

} // namespace omvr::core::stereo
