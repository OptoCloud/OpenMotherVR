#pragma once

#include <array>
#include <cstdint>

// VR input through AI.exe's XInput gamepad.
//
// MotherVR does not add an input device. It virtualises the gamepad: every
// gamepad binding in the game's three action maps is rebound to a virtual
// control code (base + action index), and five methods of the pad's vtable
// are replaced. When the game asks the pad about a virtual code, the hook
// asks the real pad about the code that was bound there originally and
// merges in the VR state the HMD backend wrote for that action.
//
// Analysis: dumps/06d_vr_input.md.

namespace mothervr::input {

// Virtual codes: map 0 (player) 0x100.., map 1 (UI) 0x200.., map 2 0x400...
// Within a map, digital action k has code base + k and analog action j has
// base + num_digital + j. Real pad codes are 0x00..0x17 (A, B, X, Y, ...).
inline constexpr int kTierBase[3] = {0x100, 0x200, 0x400};

// Per action map: the VR state the backend writes, and the pad code each
// action was bound to before the remap.
//
// From: the function-local statics behind FUN_10002ef0 / FUN_10002fa0 /
// FUN_10003050 (layout of the original's is in the analysis doc; ours need
// not match).
struct ActionTier {
    std::array<std::uint8_t, 0x2C> digital{};   // VR state of digital action k
    std::array<float, 0x10>        analog{};    // VR value of analog action j
    int base        = 0;
    int num_digital = 0;
    int num_analog  = 0;
    std::array<std::int16_t, 0x39> original_code{}; // [k] / [num_digital + j]; -1 = none
};

// The three tiers: [0] player map, [1] UI map, [2] a third map VR never
// writes (its pad bindings are still virtualised and pass through).
std::array<ActionTier, 3>& tiers() noexcept;

// Set by the backend whenever it reports a pressed digital action or an
// analog value above 0.85. Consumed by the pad's HasActivity hook, which
// makes the pad the active device and switches the button glyphs to VR.
void note_vr_activity() noexcept;

// Button-prompt glyph shown for a pad code while VR glyphs are active.
// Initially the game's pad names ("A", "B", ...); backends override some
// with "VR_*" ids that the modded UI maps to controller images. The game
// keeps the pointer it is handed, so `name` must be a string literal (or
// otherwise live for the whole process). Called on the render thread and
// read on the game thread: each name is swapped as one pointer.
void set_glyph(int pad_code, const char* name) noexcept;

// The mod's UI archive (MOTHER.PAK) is not in use this run, so the movies
// register no VR glyph ids: prompts stay in stock-glyph mode for the rest of
// the process (spec 04 section 4.2). Call once, from attach, when the
// archive redirect was not made.
void use_stock_glyphs_only() noexcept;

// The game's input devices, read from its input manager: the pad, keyboard
// and mouse the stereo update polls. Null until the game has created the
// manager, or before install().
void* pad_device() noexcept;
void* keyboard_device() noexcept;
void* mouse_device() noexcept;

// For the stereo update (FUN_1001f8a0), on the game's active pad device:
// a real pad button (pad code, no VR), the HasActivity hook, and the VR-
// merged value of a virtual analog code (e.g. 0x12C, the player map's turn).
bool  pad_button(void* pad, int pad_code) noexcept;
bool  pad_activity(void* pad) noexcept;
float pad_axis_value(void* pad, int virtual_code) noexcept;

// Saves the game's five pad methods and points the vtable slots at ours.
// `pad_vtable` is the AI.exe vtable address the original patches (one slot
// past the destructor), `input_manager` the AI.exe global holding the input
// manager pointer.
//
// The five slots are one patch (spec 04 section 3.3): with only some of
// them replaced, the first raw IsDown query would virtualise the maps and
// the stock methods left in place would get codes they do not know, so the
// real pad's sticks, triggers or prompts would stop working. If a slot
// cannot be written, the slots already written are restored and false is
// returned; the caller must then leave the pad's other patches (connected
// flag, left-stick prompt redirect) unapplied.
//
// From: FUN_1001d6f0 lines 368-397.
bool install(std::uint8_t* pad_vtable, void** input_manager) noexcept;

} // namespace mothervr::input
