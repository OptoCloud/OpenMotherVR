#pragma once

#include "core/input/types.h"

#include <cstddef>
#include <string_view>

namespace omvr::core::input::ports {

// Where the runtimes hand one frame of VR controls. Render thread.
class ControlsSink {
public:
    // Writes `c` into the player and UI action tiers, with snap-turn gating,
    // grip toggles and the flashlight-near-head gesture.
    virtual void write(const Controls& c, bool right_hand_near_head) noexcept = 0;
    // The controller layout in use (glyphs and controls help).
    virtual void set_scheme(ControlScheme scheme) noexcept = 0;
    // A hand's controller was identified (OpenVR: the device in the hand
    // role changed; OpenXR: the hand's interaction profile changed), with
    // what identifies it: the `Prop_ControllerType_String` under SteamVR
    // Input, the interaction profile path under OpenXR, empty when none is
    // bound. Hand 0 is left, 1 right. Picks the per-controller glyphs of
    // 22 §10. Render thread.
    virtual void identify_controller(std::size_t hand, ControllerSource source,
                                     std::string_view id) noexcept {
        static_cast<void>(hand), static_cast<void>(source), static_cast<void>(id);
    }

protected:
    ~ControlsSink() = default;
};

} // namespace omvr::core::input::ports
