// OpenMotherVR-Provenance: clean
#include "game/input/input_devices.h"

#include "game/code_address.h"

namespace omvr::game::input {

namespace {

// 04 §2.2: thiscall, called as __fastcall with an unused EDX; the result
// comes back in AL.
using KeyDownFn = bool(__fastcall*)(void*, void*, int);
using AnyFn     = bool(__fastcall*)(void*, void*);

} // namespace

bool key_down(const AddressMap& addresses, void* keyboard, int scancode) noexcept {
    const KeyDownFn fn = CodeAddress{addresses.at(AiAddr::KeyboardKeyDown)};
    return fn(keyboard, nullptr, scancode);
}

bool any_key(const AddressMap& addresses, void* keyboard) noexcept {
    const AnyFn fn = CodeAddress{addresses.at(AiAddr::KeyboardAnyKey)};
    return fn(keyboard, nullptr);
}

bool mouse_active(const AddressMap& addresses, void* mouse) noexcept {
    const AnyFn fn = CodeAddress{addresses.at(AiAddr::MouseActive)};
    return fn(mouse, nullptr);
}

} // namespace omvr::game::input
