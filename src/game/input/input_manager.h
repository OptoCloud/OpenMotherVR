#pragma once

#include "game/layout.h"

#include <cstddef>
#include <cstdint>

// The game's input manager and its pad device. Analysis:
// dumps/06d_vr_input.md.
namespace omvr::game::input {

// The input manager, by offset: the three action maps, and the devices the
// stereo update polls (from FUN_1001f8a0). A device's +0x11 byte is the
// game's "active device" flag.
inline constexpr std::ptrdiff_t kMapOffsets[3]  = {0x48, 0x2998, 0x3A0C};
inline constexpr std::ptrdiff_t kManagerMouse    = 0x4A70;
inline constexpr std::ptrdiff_t kManagerKeyboard = 0x4A74;
inline constexpr std::ptrdiff_t kManagerPad      = 0x4A7C;
inline constexpr std::ptrdiff_t kDeviceActive    = 0x11;

// ---- The game's pad methods (all __thiscall; __fastcall with an unused EDX) ----

using IsDownFn   = bool        (__fastcall*)(void* self, void* edx, int code);
using ValueFn    = float       (__fastcall*)(void* self, void* edx, int code);
using AxisFn     = float       (__fastcall*)(void* self, void* edx, int code, int negate);
using NameFn     = const char* (__fastcall*)(void* self, void* edx, std::uint32_t code);
using ActivityFn = bool        (__fastcall*)(void* self, void* edx);

// The stock pad methods behind the five replaced vtable slots.
struct StockPad {
    IsDownFn   is_down  = nullptr;
    ValueFn    value    = nullptr;
    AxisFn     axis     = nullptr;
    NameFn     name     = nullptr;
    ActivityFn activity = nullptr;
};

// The game's input manager, through the AI.exe global that holds it.
class InputManager {
public:
    // `input_manager` is the AI.exe global holding the manager pointer.
    void bind(void** input_manager) noexcept { g_input_manager = input_manager; }

    // The manager, null until the game has created it.
    std::uint8_t* manager() const noexcept;

    // The game's input devices, read from its input manager: the pad,
    // keyboard and mouse the stereo update polls. Null until the game has
    // created the manager, or before bind().
    void* pad_device() const noexcept;
    void* keyboard_device() const noexcept;
    void* mouse_device() const noexcept;

private:
    void* manager_field(std::ptrdiff_t offset) const noexcept;

    void** g_input_manager = nullptr;
};

// Action map `index` (0 player, 1 UI, 2 other) of the manager `mgr`.
GameActionMap& action_map(std::uint8_t* mgr, int index) noexcept;

} // namespace omvr::game::input
