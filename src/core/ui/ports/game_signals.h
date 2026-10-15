#pragma once

namespace omvr::core::ui::ports {

// One-byte requests the game's frame loop reads. Game or render thread.
class GameSignals {
public:
    // The game quits.
    virtual void request_exit() noexcept = 0;
    // The game rebuilds its render targets (as after a window resize).
    virtual void request_display_reset() noexcept = 0;
    // Whether the display-reset request is available on this build.
    virtual bool can_reset_display() const noexcept = 0;

protected:
    ~GameSignals() = default;
};

} // namespace omvr::core::ui::ports
