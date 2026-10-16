// OpenMotherVR-Provenance: clean
#pragma once

#include "core/vr/types.h"

#include <array>

// The controllers' vibration for the game's feedback (spec 21 §14): what a
// runtime plays on each hand this frame. The runtimes ask once per frame on
// the render thread, right after they read the controls (21 §14.3, §17).
namespace omvr::core::vr::ports {

// One hand's output for a frame. Amplitudes are 0 to 1, already scaled by
// Hands.Haptics.Strength.
struct HapticHand {
    // The level in effect this frame (0: none). A runtime without
    // vibration actions (OpenVR legacy input, 21 §14.3) pulses this every
    // frame while it is above 0.
    float level = 0.0f;
    // Start a vibration now, replacing any that is playing: `amplitude`
    // for `seconds` (runtimes with vibration actions).
    bool play       = false;
    float amplitude = 0.0f;
    float seconds   = 0.0f;
    // A vibration still playing must end now (a continuous level dropped to
    // 0, the hand was lost, input focus went).
    bool stop = false;
};

using HapticFrame = std::array<HapticHand, kHandCount>;

class HapticSource {
public:
    // Render thread, once per frame. `focus`: the runtime gives this
    // application input (no dashboard over it); `tracked`: each hand's
    // controller is tracked. Without either a hand gets nothing (21 §14.3).
    virtual HapticFrame next(bool focus, const std::array<bool, kHandCount>& tracked) noexcept = 0;

protected:
    ~HapticSource() = default;
};

} // namespace omvr::core::vr::ports
