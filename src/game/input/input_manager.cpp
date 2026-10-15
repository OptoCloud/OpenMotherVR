// From: FUN_1001f8a0 (the device offsets) and FUN_100031b0 (the map
// offsets). Analysis: dumps/06d_vr_input.md.

#include "game/input/input_manager.h"

namespace omvr::game::input {

std::uint8_t* InputManager::manager() const noexcept {
    return g_input_manager ? static_cast<std::uint8_t*>(*g_input_manager) : nullptr;
}

GameActionMap& action_map(std::uint8_t* mgr, int index) noexcept {
    return *reinterpret_cast<GameActionMap*>(mgr + kMapOffsets[index]);
}

void* InputManager::manager_field(std::ptrdiff_t offset) const noexcept {
    std::uint8_t* mgr = manager();
    return mgr ? *reinterpret_cast<void**>(mgr + offset) : nullptr;
}

void* InputManager::pad_device() const noexcept { return manager_field(kManagerPad); }
void* InputManager::keyboard_device() const noexcept { return manager_field(kManagerKeyboard); }
void* InputManager::mouse_device() const noexcept { return manager_field(kManagerMouse); }

} // namespace omvr::game::input
