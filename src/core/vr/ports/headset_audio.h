#pragma once

#include <string>

namespace omvr::core::vr::ports {

// Plays the game's audio on the headset's own output device.
class HeadsetAudio {
public:
    // `endpoint_id` is an MMDevice endpoint ID as the runtime reports it.
    // Empty: nothing to do. Takes effect once; later calls are ignored.
    virtual void redirect_to(std::wstring endpoint_id) noexcept = 0;

protected:
    ~HeadsetAudio() = default;
};

} // namespace omvr::core::vr::ports
