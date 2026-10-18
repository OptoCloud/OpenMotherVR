// OpenMotherVR-Provenance: clean
#pragma once

namespace omvr::core::input::ports {

// What the game is doing, as the gestures of 22 §8.4 and §9 need it: a menu
// on the virtual screen (stereo mode 1, 05 §1), an interaction, hiding and
// the hiding place's breathing game (11 §5.3, 22 §9.1). Read on the render
// thread; the state behind it is written on the game thread, each value a
// single atomic word.
struct GameContext {
    bool menu           = false;
    bool interaction    = false;
    bool hiding         = false;
    bool breathing_game = false;
};

class GestureContext {
public:
    [[nodiscard]] virtual GameContext game_context() const noexcept = 0;

protected:
    ~GestureContext() = default;
};

} // namespace omvr::core::input::ports
