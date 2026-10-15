#include "game/sites/controls.h"

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "game/patcher.h"

#include <cstddef>
#include <iterator>

namespace omvr::game::sites {

namespace {

namespace diag = core::diag;
namespace hook {
using game::write_dword;
} // namespace hook
using input::ActivityFn;
using input::AxisFn;
using input::IsDownFn;
using input::NameFn;
using input::ValueFn;

template <class Fn>
Fn read_slot(const std::uint8_t* slot) noexcept {
    return *reinterpret_cast<Fn const*>(slot);
}

// A failed write is logged by write_dword; that slot keeps the game's function.
bool write_slot(std::uint8_t* slot, const void* fn) noexcept {
    return hook::write_dword(slot, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(fn)));
}

// Five pad slots, the connected flag and two prompt calls.
constexpr int kControlSites = 8;

} // namespace

bool install_pad_slots(std::uint8_t* pad_vtable, const PadReplacements& replacements,
                       input::StockPad& g_game) noexcept {
    // The original keeps these in DAT_10066c6c..c7c (the same addresses,
    // from its table); reading them from the vtable is equivalent.
    g_game.is_down  = read_slot<IsDownFn>(pad_vtable + 0x00);
    g_game.value    = read_slot<ValueFn>(pad_vtable + 0x04);
    g_game.axis     = read_slot<AxisFn>(pad_vtable + 0x08);
    g_game.name     = read_slot<NameFn>(pad_vtable + 0x10);
    g_game.activity = read_slot<ActivityFn>(pad_vtable + 0x28);
    // The game calls the replacements as soon as a slot is written, so the
    // stock methods are in place first.

    struct Slot {
        std::ptrdiff_t offset;
        const void*    fn;
    };
    const Slot slots[] = {
        {0x00, replacements.is_down},
        {0x04, replacements.value},
        {0x08, replacements.axis},
        {0x10, replacements.control_name},
        {0x28, replacements.has_activity},
    };
    std::uint32_t saved[std::size(slots)] = {};
    for (std::size_t i = 0; i < std::size(slots); ++i) {
        saved[i] = *reinterpret_cast<const std::uint32_t*>(pad_vtable + slots[i].offset);
    }

    // All or nothing (see gamepad.h): on a failed write, put back the slots
    // already written so the game runs with its stock pad.
    std::size_t written = 0;
    while (written < std::size(slots) &&
           write_slot(pad_vtable + slots[written].offset, slots[written].fn)) {
        ++written;
    }
    if (written < std::size(slots)) {
        bool restored = true;
        for (std::size_t i = 0; i < written; ++i) {
            restored = hook::write_dword(pad_vtable + slots[i].offset, saved[i]) && restored;
        }
        diag::logf("[input] gamepad vtable slot +0x{:X} could not be written; {}: VR controls are off, "
                   "the game keeps its stock pad",
                   slots[written].offset, restored ? "earlier slots restored" : "restoring earlier slots FAILED");
        return false;
    }


    diag::logf("[input] gamepad vtable at {} hooked", static_cast<void*>(pad_vtable));
    return true;
}

int install_controls(std::uint8_t* game_base, const InlineSites& sites, const PadReplacements& replacements,
                     input::StockPad& stock) noexcept {
    if (!install_pad_slots(ai_address(AiAddr::PadVtable), replacements, stock)) {
        return 0;
    }
    const bool ok =
        // The pad always counts as connected: VR controllers come through it.
        patch(ai_address(AiAddr::PadConnectedSet), {0xB1, 0x01, kNop}) &&
        redirect_call(ai_address(AiAddr::LeftStickPromptCall), replacements.move_on_left_stick,
                      "left stick prompt") &&
        redirect_call(game_base + sites.left_stick_prompt_call_2, replacements.move_on_left_stick,
                      "left stick prompt");
    return ok ? kControlSites : -1;
}

} // namespace omvr::game::sites
