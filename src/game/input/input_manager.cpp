// From: FUN_1001f8a0 (the device offsets) and FUN_100031b0 (the map
// offsets). Analysis: dumps/06d_vr_input.md.

#include "game/input/input_manager.h"

namespace omvr::game::input {

namespace {

// The pointer stored at `offset` in the game object `base`.
void* pointer_at(std::uint8_t* base, std::ptrdiff_t offset) noexcept {
    return *reinterpret_cast<void**>(base + offset);
}

} // namespace

std::uint8_t* InputManager::manager() const noexcept {
    return manager_global_ ? static_cast<std::uint8_t*>(*manager_global_) : nullptr;
}

GameActionMap& action_map(std::uint8_t* mgr, std::size_t index) noexcept {
    return *reinterpret_cast<GameActionMap*>(mgr + kInputManagerMapOffsets[index]);
}

void* InputManager::manager_field(std::ptrdiff_t offset) const noexcept {
    std::uint8_t* mgr = manager();
    return mgr ? pointer_at(mgr, offset) : nullptr;
}

void* InputManager::pad_device() const noexcept { return manager_field(kInputManagerPad); }
void* InputManager::keyboard_device() const noexcept { return manager_field(kInputManagerKeyboard); }
void* InputManager::mouse_device() const noexcept { return manager_field(kInputManagerMouse); }

std::optional<ActionMaps> InputManager::action_maps() const noexcept {
    std::uint8_t* mgr = manager();
    if (!mgr) return std::nullopt;
    ActionMaps maps{};
    for (std::size_t i = 0; i < maps.size(); ++i) maps[i] = &action_map(mgr, i);
    return maps;
}

std::optional<PlayerDevices> InputManager::player_devices() const noexcept {
    std::uint8_t* mgr = manager();
    if (!mgr) return std::nullopt;
    void* const* devices = action_map(mgr, 0).device_by_type;
    return PlayerDevices{static_cast<std::uint8_t*>(devices[kDeviceTypePad]),
                         static_cast<std::uint8_t*>(devices[kDeviceTypeKeyboard])};
}

void set_device_active(std::uint8_t* device, bool active) noexcept {
    device[kInputDeviceActive] = static_cast<std::uint8_t>(active);
}

} // namespace omvr::game::input
