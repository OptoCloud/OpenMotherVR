#pragma once

#include "core/diag/ports/log.h"
#include "core/input/action_tiers.h"
#include "core/input/glyphs.h"
#include "core/input/remap.h"
#include "core/input/types.h"
#include "util/sync/once_latch.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace omvr::core::input {

// Which of map 0's pad and keyboard devices exist (04 §4.2): the prompt
// switch writes through them only when they do.
enum class PlayerDevices {
    NoManager, // the game has not created its input manager
    Missing,   // the manager exists; the pad or the keyboard entry is null
    Both,
};

// What the pad thunk writes to the game's devices after a HasActivity
// query, to make the game redraw its prompts (04 §4.2).
enum class DeviceSwitch {
    None,
    // The mode changed: mark the pad inactive and the keyboard active, so
    // the game's device selection activates the pad again and refreshes.
    PulseKeyboard,
    // The call after a pulse: the pad active again, the keyboard not (each
    // only where it exists).
    RestorePad,
};

struct ActivityResult {
    bool any             = false; // the merged HasActivity answer
    DeviceSwitch devices = DeviceSwitch::None;
};

// The virtualised gamepad (04 §3): the VR state of every action, the merge
// of the game's pad with it behind the five replaced pad methods, and the
// remap of the game's action maps (see action_tiers.h). Pure: the thunks in
// loader/hooks/pad_hooks.cpp read the game's pad and maps and pass the
// values in.
//
// From: FUN_10008d50 (IsDown), FUN_10008f00 (GetValue), FUN_10008e20
// (GetAxis), FUN_10009030 (ControlToName), FUN_10008fa0 (HasActivity),
// FUN_100031b0 / FUN_10003200 (action-map remap).
class VirtualPad final {
public:
    VirtualPad(const Glyphs& glyphs, diag::ports::Log& log) noexcept : glyphs_(glyphs), log_(log) {}

    // The three tiers the runtimes write.
    [[nodiscard]] ActionTiers& tiers() noexcept { return tiers_; }

    // Set by the runtime whenever it reports a pressed digital action or an
    // analog value above kActivityThreshold. Consumed by the pad's
    // HasActivity hook, which makes the pad the active device and switches
    // the button glyphs to VR. Render thread.
    void note_vr_activity() noexcept { vr_activity_.store(true); }

    // This frame's d-pad press from the VR controllers: a controller's own
    // d-pad and the emulation (22 §7). Every digital action of maps 0 and 1
    // whose stock pad control is that direction reads it, merged by OR.
    // Render thread.
    void set_dpad(Dpad d) noexcept;

    // The mod's UI archive (MOTHER.PAK) is not in use this run, so the movies
    // register no VR glyph ids: prompts stay in stock-glyph mode for the rest
    // of the process (spec 04 section 4.2). Call once, from attach, when the
    // archive redirect was not made.
    void use_stock_glyphs_only() noexcept;

    // All five pad slots were replaced. The helpers for the stereo update
    // answer from the stock pad alone when they were not.
    void mark_installed() noexcept { installed_.store(true); }
    [[nodiscard]] bool installed() const noexcept { return installed_.load(); }

    // ---- The replaced pad methods' decisions (04 §3.3). Game thread. ------

    // A raw code (not virtual) that is not "no control" means the maps were
    // (re)loaded and need virtualising (04 §3.2).
    [[nodiscard]] static bool asks_for_remap(int code) noexcept {
        return !is_virtual_code(code) && code != game::kNoPadControl;
    }

    // The pad control virtual `code` stood for, kNoPadControl for none.
    [[nodiscard]] int stock_code(int code) const noexcept { return input::stock_code(tiers_, code); }

    // IsDown of a virtual code: the real pad's answer for the stock control
    // or, for a digital action, its VR state.
    [[nodiscard]] bool merge_down(int code, bool stock_down) const noexcept;

    // GetValue of a virtual code: non-negative, the larger of pad and VR.
    [[nodiscard]] float merge_value(int code, float stock_value) const noexcept;

    // GetAxis of a virtual code: signed, whichever has the larger magnitude.
    // The VR value is not negated: the runtimes write signed values.
    [[nodiscard]] float merge_axis(int code, float stock_axis) const noexcept;

    // ControlToName: whether the prompts show VR glyphs (else the stock name
    // of the stock control), and the VR glyph for a stock control.
    [[nodiscard]] bool shows_vr_glyphs() const noexcept;
    [[nodiscard]] const char* vr_glyph(int stock) const noexcept;

    // HasActivity: VR activity counts as pad activity. When the source flips
    // between VR and the pad, the result asks for the pad to be marked
    // inactive for one call so the game runs its device-changed path and
    // refreshes the button prompts (04 §4.2).
    [[nodiscard]] ActivityResult on_activity(bool stock_activity, PlayerDevices devices) noexcept;

    // One virtualisation pass over the three action maps; logs the counts
    // once. From: FUN_100031b0.
    template <ActionMap Map>
    void remap(const std::array<Map*, kTierCount>& maps) noexcept {
        for (std::size_t i = 0; i < kTierCount; ++i) input::remap(tiers_[i], *maps[i]);
        if (remap_logged_.first()) log_remap();
    }

    // ---- For the stereo update (FUN_1001f8a0). Game thread. ----------------

    // The virtual code of analog action `index` of map `map` (e.g. the
    // player map's turn), or kNoPadControl.
    [[nodiscard]] int analog_virtual_code(int map, int index) const noexcept {
        return input::analog_virtual_code(tiers_, map, index);
    }
    [[nodiscard]] bool move_on_left_stick() const noexcept { return input::move_on_left_stick(tiers_); }

private:
    void log_remap() noexcept;
    // Digital action `code` of map 0 or 1 had a d-pad direction now pressed
    // from VR as its stock pad control.
    [[nodiscard]] bool dpad_down(int code) const noexcept;

    const Glyphs& glyphs_;
    diag::ports::Log& log_;
    ActionTiers tiers_ = make_tiers();
    // Original: DAT_10066bc0 (initially 1), DAT_10068639, DAT_1006af7c.
    // Game thread, except vr_activity_ (set on the render thread).
    bool vr_glyphs_ = true;
    std::atomic<bool> vr_activity_{false};
    // The VR d-pad press, one bit per direction (kDpadBit*); render thread to
    // game thread. Relaxed: the bits are independent and nothing else hangs
    // on them, as with the tiers' VR state (04 §3.6).
    std::atomic<std::uint8_t> dpad_{0};
    bool device_pulse_ = false;
    // False when the mod's UI archive is not in use this run: the stock
    // movies register no VR glyph ids, so a VR name would show no button at
    // all (spec 04 section 4.2). Set at attach, read on the game thread.
    std::atomic<bool> vr_glyphs_allowed_{true};
    std::atomic<bool> installed_{false};
    util::OnceLatch remap_logged_;
};

} // namespace omvr::core::input
