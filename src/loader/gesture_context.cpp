// OpenMotherVR-Provenance: clean
#include "loader/gesture_context.h"

#include "game/constants.h"

#include <atomic>
#include <cstdint>

namespace omvr::loader {

core::input::ports::GameContext GestureContextView::game_context() const noexcept {
    // The game writes the mode on its own thread through its push and pop;
    // an aligned 32-bit read sees one whole value. Relaxed: a frame late is
    // harmless, nothing else is read with it.
    const std::int32_t mode = std::atomic_ref<std::int32_t>(manager_.mode).load(std::memory_order_relaxed);
    return {
        .menu           = mode == game::kStereoModeScreen,
        .interaction    = interactions_.in_interaction(),
        .hiding         = interactions_.hiding(),
        .breathing_game = interactions_.breathing_game(),
    };
}

} // namespace omvr::loader
