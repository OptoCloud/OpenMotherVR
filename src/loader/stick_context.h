// OpenMotherVR-Provenance: clean
#pragma once

#include "core/input/interaction_state.h"
#include "core/input/ports/stick_context.h"
#include "game/layout.h"

namespace omvr::loader {

// Whether the left stick walks the player (22 §7), over the stereo mode the
// game keeps in the StereoManager the mod hands it (05 §4.4) and the script
// events' interaction state (11 §5.3). Render thread.
class StickContextView final : public core::input::ports::StickContext {
public:
    StickContextView(game::StereoManager& manager, const core::input::InteractionState& interactions) noexcept
        : manager_(manager), interactions_(interactions) {}

    bool stick_free() const noexcept override;

private:
    game::StereoManager& manager_;
    const core::input::InteractionState& interactions_;
};

} // namespace omvr::loader
