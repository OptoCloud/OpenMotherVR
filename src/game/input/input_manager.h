#pragma once

#include "game/layout.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// The game's input manager and its pad device (04 §2.1 to §2.3). Analysis:
// dumps/06d_vr_input.md.
namespace omvr::game::input {

// The input manager's offsets (the three action maps, and the devices the
// stereo update polls, from FUN_1001f8a0) are in game/layout.h.

// ---- The game's pad methods (all __thiscall; __fastcall with an unused EDX) ----

using IsDownFn   = bool(__fastcall*)(void* self, void* edx, int code);
using ValueFn    = float(__fastcall*)(void* self, void* edx, int code);
using AxisFn     = float(__fastcall*)(void* self, void* edx, int code, int negate);
using NameFn     = const char*(__fastcall*)(void* self, void* edx, std::uint32_t code);
using ActivityFn = bool(__fastcall*)(void* self, void* edx);

// The stock pad methods behind the five replaced vtable slots (04 §2.3).
struct StockPad {
    IsDownFn is_down    = nullptr;
    ValueFn value       = nullptr;
    AxisFn axis         = nullptr;
    NameFn name         = nullptr;
    ActivityFn activity = nullptr;
};

// The three action maps of the manager: 0 player, 1 menus, 2 other.
using ActionMaps = std::array<GameActionMap*, kActionMapCount>;

// Map 0's pad and keyboard devices (device_by_type[0], [1]). An entry stays
// null until the game has registered a device of that type (04 §4.2).
struct PlayerDevices {
    std::uint8_t* pad      = nullptr;
    std::uint8_t* keyboard = nullptr;
};

// The game's input manager, through the AI.exe global that holds it. Game
// thread.
class InputManager {
public:
    // `input_manager` is the AI.exe global holding the manager pointer.
    void bind(void** input_manager) noexcept { manager_global_ = input_manager; }

    // The manager, null until the game has created it (or before bind()).
    [[nodiscard]] std::uint8_t* manager() const noexcept;

    // The game's input devices, read from its input manager: the pad,
    // keyboard and mouse the stereo update polls. Null until the game has
    // created the manager, or before bind().
    [[nodiscard]] void* pad_device() const noexcept;
    [[nodiscard]] void* keyboard_device() const noexcept;
    [[nodiscard]] void* mouse_device() const noexcept;

    // The action maps, or nullopt until the game has created the manager.
    [[nodiscard]] std::optional<ActionMaps> action_maps() const noexcept;

    // Map 0's pad and keyboard, or nullopt until the game has created the
    // manager.
    [[nodiscard]] std::optional<PlayerDevices> player_devices() const noexcept;

private:
    [[nodiscard]] void* manager_field(std::ptrdiff_t offset) const noexcept;

    // Lives in AI.exe's data for the whole process.
    void** manager_global_ = nullptr;
};

// Action map `index` (0 player, 1 UI, 2 other) of the manager `mgr`.
[[nodiscard]] GameActionMap& action_map(std::uint8_t* mgr, std::size_t index) noexcept;

// Sets or clears a device's "is the active device" byte (04 §2.2). `device`
// must not be null.
void set_device_active(std::uint8_t* device, bool active) noexcept;

} // namespace omvr::game::input
