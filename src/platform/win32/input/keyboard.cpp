#include "platform/win32/input/keyboard.h"

#include "platform/win32/constants.h"

#include <windows.h>

namespace omvr::platform::win32 {

namespace {

bool held(int virtual_key) noexcept { return (::GetAsyncKeyState(virtual_key) & kKeyDownBit) != 0; }

} // namespace

bool Keyboard::ctrl_and_alt_held() const noexcept { return held(VK_CONTROL) && held(VK_MENU); }

bool Keyboard::key_down(core::stereo::ports::Key key) const noexcept {
    switch (key) {
        case core::stereo::ports::Key::F10: return held(VK_F10);
    }
    return false;
}

} // namespace omvr::platform::win32
