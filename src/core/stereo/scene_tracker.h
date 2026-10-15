#pragma once

#include <cstdint>

namespace omvr::core::stereo {

// A cutscene or level fade is running; the camera left the player during
// it. Original: DAT_1006863a, DAT_1006863b. Game thread.
struct SceneTracker {
    bool g_cutscene_or_fade = false;
    bool g_cutscene_took_camera = false;

    // A UI entity message: cutscene and level-fade start/finish switch the
    // tracker; `id` is the message's name hash.
    // From: FUN_1001f830.
    void on_message(std::uint32_t id) noexcept;
};

} // namespace omvr::core::stereo
