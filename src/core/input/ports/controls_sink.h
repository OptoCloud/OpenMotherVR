#pragma once

#include "core/input/types.h"

namespace omvr::core::input::ports {

// Where the runtimes hand one frame of VR controls. Render thread.
class ControlsSink {
public:
    // Writes `c` into the player and UI action tiers, with snap-turn gating,
    // grip toggles and the flashlight-near-head gesture.
    virtual void write(const Controls& c, bool right_hand_near_head) noexcept = 0;
    // The controller layout in use (glyphs and controls help).
    virtual void set_scheme(ControlScheme scheme) noexcept = 0;

protected:
    ~ControlsSink() = default;
};

} // namespace omvr::core::input::ports
