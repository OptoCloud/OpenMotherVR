#pragma once

#include "core/stereo/ports/keyboard.h"

namespace omvr::platform::win32 {

// The physical keyboard through GetAsyncKeyState, whatever window has the
// focus. Any thread.
class Keyboard final : public core::stereo::ports::Keyboard {
public:
    [[nodiscard]] bool ctrl_and_alt_held() const noexcept override;
    [[nodiscard]] bool key_down(core::stereo::ports::Key key) const noexcept override;
};

} // namespace omvr::platform::win32
