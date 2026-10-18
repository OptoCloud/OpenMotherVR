#pragma once

#include "game/game_image.h"
#include "game/input/input_manager.h"
#include "game/patcher.h"

#include <array>
#include <cstdint>

namespace omvr::game::sites {

// The replacements for the pad's five methods (vtable slots 1, 2, 3, 5 and
// 11, at +0x00, +0x04, +0x08, +0x10 and +0x28 from slot 1) and for the
// left-stick prompt check.
struct PadReplacements {
    const void* is_down;
    const void* value;
    const void* axis;
    const void* control_name;
    const void* has_activity;
    const void* move_on_left_stick;
};

// A pad slot the controls replace: the table's PadVtable is one slot past
// the destructor, so slot n is entry n - kPadFirstTableSlot. `keep_stock`
// stores the game's method into StockPad.
struct PadSlot {
    SlotSite site;
    const void* PadReplacements::* replacement;
    void (*keep_stock)(input::StockPad& stock, CodeAddress method) noexcept;
};

// The five slots, in the order they are written.
inline constexpr std::array kPadSlots = {
    PadSlot{{AiAddr::PadVtable, kPadIsDownSlot - kPadFirstTableSlot, AiAddr::PadIsDown,
             "the pad's IsDown (slot 1)"},
            &PadReplacements::is_down,
            [](input::StockPad& s, CodeAddress m) noexcept { s.is_down = m; }},
    PadSlot{{AiAddr::PadVtable, kPadGetValueSlot - kPadFirstTableSlot, AiAddr::PadGetValue,
             "the pad's GetValue (slot 2)"},
            &PadReplacements::value,
            [](input::StockPad& s, CodeAddress m) noexcept { s.value = m; }},
    PadSlot{{AiAddr::PadVtable, kPadGetAxisSlot - kPadFirstTableSlot, AiAddr::PadGetAxis,
             "the pad's GetAxis (slot 3)"},
            &PadReplacements::axis,
            [](input::StockPad& s, CodeAddress m) noexcept { s.axis = m; }},
    PadSlot{{AiAddr::PadVtable, kPadControlNameSlot - kPadFirstTableSlot, AiAddr::PadControlToName,
             "the pad's ControlToName (slot 5)"},
            &PadReplacements::control_name,
            [](input::StockPad& s, CodeAddress m) noexcept { s.name = m; }},
    PadSlot{{AiAddr::PadVtable, kPadHasActivitySlot - kPadFirstTableSlot, AiAddr::PadHasActivity,
             "the pad's HasActivity (slot 11)"},
            &PadReplacements::has_activity,
            [](input::StockPad& s, CodeAddress m) noexcept { s.activity = m; }},
};

// Saves the game's five pad methods into `stock` and points the vtable slots
// at ours. `pad_vtable` is the AI.exe vtable address the table names (one
// slot past the destructor).
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
bool install_pad_slots(Patcher& patcher, std::uint8_t* pad_vtable, const PadReplacements& replacements,
                       input::StockPad& stock) noexcept;

// What the controls step did.
enum class ControlsOutcome {
    SlotsNotWritten,  // the pad stays stock; nothing of the step is in place
    LaterWriteFailed, // the slots are in, a later write of the step failed
    Installed,
};

// Sites of the controls step: five pad slots, the connected flag and two
// prompt calls.
inline constexpr int kLeftStickPromptCalls = 2;
inline constexpr int kControlSites         = static_cast<int>(kPadSlots.size()) + 1 + kLeftStickPromptCalls;
static_assert(kControlSites == 8);

// 9, controls: VR input goes through the game's gamepad, five of its vtable
// slots replaced as one patch. Only with those in place does the pad count
// as always connected and do the left-stick prompts ask the remapped
// bindings; without them the real pad keeps the game's own behaviour
// (spec 04 section 3.3).
ControlsOutcome install_controls(const GameImage& g, Patcher& patcher, const PadReplacements& replacements,
                                 input::StockPad& stock) noexcept;

} // namespace omvr::game::sites
