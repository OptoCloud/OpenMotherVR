#include "game/sites/controls.h"

#include "game/code_address.h"
#include "game/x86.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace omvr::game::sites {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kSlotFailedLog =
    "[input] gamepad vtable slot +0x{:X} could not be written; {}: VR controls are off, the game keeps its "
    "stock pad";
constexpr std::string_view kRestoredText     = "earlier slots restored";
constexpr std::string_view kNotRestoredText  = "restoring earlier slots FAILED";
constexpr std::string_view kPadHookedLog     = "[input] gamepad vtable at {} hooked";
constexpr std::string_view kLeftStickPrompt  = "left stick prompt";

// 05 §7.10: `setne cl` becomes `mov cl, 1; nop`: the pad always counts as
// connected, since VR controllers come through it.
constexpr std::array<std::uint8_t, 3> kPadAlwaysConnected = {x86::kMovClImm8, 0x01, x86::kNop};

// Byte offset of a pad slot from the table's vtable address.
constexpr std::uint32_t pad_slot_offset(std::uint32_t slot) noexcept {
    return slot_offset(slot - kPadFirstTableSlot);
}

// The five replaced pad methods.
constexpr std::size_t kPadSlotCount = 5;

struct Slot {
    std::uint32_t offset;
    const void*   fn;
};

// A failed write is logged by the patcher; that slot keeps the game's
// method. On a failure the slots already written are put back, so the game
// runs with its stock pad. Returns the index of the slot that failed, or
// the count when all were written.
template <std::size_t N>
std::size_t write_slots(Patcher& p, std::uint8_t* vtable, const std::array<Slot, N>& slots,
                        const std::array<std::uint32_t, N>& saved, bool& restored) noexcept {
    std::size_t written = 0;
    while (written < N && p.write_pointer(vtable + slots[written].offset, slots[written].fn)) ++written;
    restored = true;
    if (written < N) {
        for (std::size_t i = 0; i < written; ++i) {
            restored = p.write_u32(vtable + slots[i].offset, saved[i]).has_value() && restored;
        }
    }
    return written;
}

} // namespace

bool install_pad_slots(Patcher& p, std::uint8_t* pad_vtable, const PadReplacements& replacements,
                       input::StockPad& stock) noexcept {
    // The game calls the replacements as soon as a slot is written, so the
    // stock methods are in place first. These are the same addresses the
    // table names (PadIsDown..PadHasActivity), which the preflight checked.
    stock.is_down  = pointer_at(pad_vtable + pad_slot_offset(kPadIsDownSlot));
    stock.value    = pointer_at(pad_vtable + pad_slot_offset(kPadGetValueSlot));
    stock.axis     = pointer_at(pad_vtable + pad_slot_offset(kPadGetAxisSlot));
    stock.name     = pointer_at(pad_vtable + pad_slot_offset(kPadControlNameSlot));
    stock.activity = pointer_at(pad_vtable + pad_slot_offset(kPadHasActivitySlot));

    const std::array<Slot, kPadSlotCount> slots = {{
        {pad_slot_offset(kPadIsDownSlot), replacements.is_down},
        {pad_slot_offset(kPadGetValueSlot), replacements.value},
        {pad_slot_offset(kPadGetAxisSlot), replacements.axis},
        {pad_slot_offset(kPadControlNameSlot), replacements.control_name},
        {pad_slot_offset(kPadHasActivitySlot), replacements.has_activity},
    }};
    std::array<std::uint32_t, kPadSlotCount> saved{};
    for (std::size_t i = 0; i < kPadSlotCount; ++i) saved[i] = read_u32(pad_vtable + slots[i].offset);

    // All or nothing (04 §3.3).
    bool restored = true;
    const std::size_t written = write_slots(p, pad_vtable, slots, saved, restored);
    if (written < kPadSlotCount) {
        ports::logf(p.log(), kSlotFailedLog, slots[written].offset, restored ? kRestoredText : kNotRestoredText);
        return false;
    }
    ports::logf(p.log(), kPadHookedLog, static_cast<void*>(pad_vtable));
    return true;
}

ControlsOutcome install_controls(const GameImage& g, Patcher& p, const PadReplacements& replacements,
                                 input::StockPad& stock) noexcept {
    if (!install_pad_slots(p, g.at(AiAddr::PadVtable), replacements, stock)) return ControlsOutcome::SlotsNotWritten;
    const bool ok =
        p.write(g.at(AiAddr::PadConnectedSet), kPadAlwaysConnected) &&
        p.redirect_call(g.at(AiAddr::LeftStickPromptCall), replacements.move_on_left_stick, kLeftStickPrompt) &&
        p.redirect_call(g.at_rva(g.sites.left_stick_prompt_call_2), replacements.move_on_left_stick,
                        kLeftStickPrompt);
    return ok ? ControlsOutcome::Installed : ControlsOutcome::LaterWriteFailed;
}

} // namespace omvr::game::sites
