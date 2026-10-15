#include "game/input/input_devices.h"

#include "game/addresses.h"

namespace omvr::game::input {

namespace {

using KeyDownFn = bool (__fastcall*)(void*, void*, int);
using AnyFn     = bool (__fastcall*)(void*, void*);

template <class Fn>
Fn game_fn(AiAddr slot) noexcept { return reinterpret_cast<Fn>(ai_address(slot)); }

} // namespace

bool key_down(void* keyboard, int scancode) noexcept {
    return game_fn<KeyDownFn>(AiAddr::KeyboardKeyDown)(keyboard, nullptr, scancode);
}

bool any_key(void* keyboard) noexcept { return game_fn<AnyFn>(AiAddr::KeyboardAnyKey)(keyboard, nullptr); }

bool mouse_active(void* mouse) noexcept { return game_fn<AnyFn>(AiAddr::MouseActive)(mouse, nullptr); }

} // namespace omvr::game::input
