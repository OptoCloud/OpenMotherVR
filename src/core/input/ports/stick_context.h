// OpenMotherVR-Provenance: clean
#pragma once

namespace omvr::core::input::ports {

// Whether the left stick walks the player now (22 §7): it does not during an
// interaction or the hiding place's breathing game (11 §5.3, 22 §9.1), nor
// while a menu is on the virtual screen (stereo mode 1, 05 §4.4). Read on
// the render thread; the state behind it is written on the game thread.
class StickContext {
public:
    [[nodiscard]] virtual bool stick_free() const noexcept = 0;

protected:
    ~StickContext() = default;
};

} // namespace omvr::core::input::ports
