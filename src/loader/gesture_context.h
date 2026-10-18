// OpenMotherVR-Provenance: clean
#pragma once

#include "core/input/interaction_state.h"
#include "core/input/ports/gesture_context.h"
#include "game/layout.h"

namespace omvr::loader {

// What the game is doing for the gestures of 22 §8.4 and §9, over the
// stereo mode the game keeps in the StereoManager the mod hands it (05 §4.4)
// and the script events' interaction state (11 §5.3). Render thread.
class GestureContextView final : public core::input::ports::GestureContext {
public:
    GestureContextView(game::StereoManager& manager,
                       const core::input::InteractionState& interactions) noexcept
        : manager_(manager), interactions_(interactions) {}

    core::input::ports::GameContext game_context() const noexcept override;

private:
    game::StereoManager& manager_;
    const core::input::InteractionState& interactions_;
};

} // namespace omvr::loader
