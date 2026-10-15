#include "platform/win32/input/keyboard.h"

#include <windows.h>

namespace omvr::platform::win32 {

bool Keyboard::ctrl_and_alt_held() const noexcept {
    return (::GetAsyncKeyState(VK_CONTROL) & 0x8000) && (::GetAsyncKeyState(VK_MENU) & 0x8000);
}

bool Keyboard::key_down(core::stereo::ports::Key key) const noexcept {
    switch (key) {
        case core::stereo::ports::Key::F10: return (::GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    }
    return false;
}

} // namespace omvr::platform::win32
