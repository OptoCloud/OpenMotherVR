#pragma once

namespace omvr::core::stereo::ports {

// Keys the mod reads itself, outside the game's input.
enum class Key { F10 };

// The physical keyboard. Any thread.
class Keyboard {
public:
    // Ctrl and Alt are both held (the recenter chord).
    virtual bool ctrl_and_alt_held() const noexcept = 0;
    virtual bool key_down(Key key) const noexcept = 0;

protected:
    ~Keyboard() = default;
};

} // namespace omvr::core::stereo::ports
