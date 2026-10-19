// VR input through AI.exe's XInput gamepad.
//
// From: FUN_10008d50 (IsDown), FUN_10008f00 (GetValue), FUN_10008e20
// (GetAxis), FUN_10009030 (ControlToName), FUN_10008fa0 (HasActivity),
// FUN_100031b0 / FUN_10003200 (action-map remap), FUN_10003130 (code
// lookup), FUN_10001090 (glyph names). Analysis: dumps/06d_vr_input.md.

#include "core/input/virtual_pad.h"

#include "core/input/glyph_ids.h"

#include <cmath>
#include <cstddef>
#include <string_view>

namespace omvr::core::input {

namespace {

constexpr std::string_view kRemapLog = "[input] action maps virtualised: digital/analog {}/{} {}/{} {}/{}";
constexpr std::string_view kStockGlyphsLog =
    "[input] no mod UI archive: button prompts keep the game's pad glyphs";

// The VR d-pad press as bits: bit (code - DPAD_UP) for each direction's pad
// code (04 §2.3: DPAD_UP..DPAD_RIGHT are 0x04..0x07).
constexpr std::uint8_t dpad_bit(int pad_code) noexcept {
    return static_cast<std::uint8_t>(1u << static_cast<unsigned>(pad_code - game::kPadDpadUp));
}

} // namespace

// From: FUN_100031b0.
void VirtualPad::log_remap() noexcept {
    diag::ports::logf(log_, kRemapLog, tiers_[0].num_digital, tiers_[0].num_analog, tiers_[1].num_digital,
                      tiers_[1].num_analog, tiers_[2].num_digital, tiers_[2].num_analog);
}

// bool IsDown(int code). From: FUN_10008d50 (ret 4).
bool VirtualPad::merge_down(int code, bool stock_down) const noexcept {
    const ActionTier& t = tier_for(tiers_, code);
    const int k         = code - t.base;
    const bool vr_down  = k >= 0 && k < t.num_digital && vr_digital(t, static_cast<std::size_t>(k));
    return stock_down || vr_down || dpad_down(code);
}

void VirtualPad::set_dpad(Dpad d) noexcept {
    std::uint8_t bits = 0;
    if (d.up) bits |= dpad_bit(game::kPadDpadUp);
    if (d.down) bits |= dpad_bit(game::kPadDpadDown);
    if (d.left) bits |= dpad_bit(game::kPadDpadLeft);
    if (d.right) bits |= dpad_bit(game::kPadDpadRight);
    dpad_.store(bits, std::memory_order_relaxed);
}

// 22 §7: by the action's stock control, so whatever the player bound to the
// d-pad follows it; map 2 gets no VR state (04 §5.8.5).
bool VirtualPad::dpad_down(int code) const noexcept {
    const std::uint8_t bits = dpad_.load(std::memory_order_relaxed);
    if (bits == 0 || code >= game::kTierBase[kTierCount - 1]) return false; // map 2
    const ActionTier& t = tier_for(tiers_, code);
    const int k         = code - t.base;
    if (k < 0 || k >= t.num_digital) return false;
    const int stock = t.stock_code[static_cast<std::size_t>(k)];
    return is_dpad_code(stock) && (bits & dpad_bit(stock)) != 0;
}

// float GetValue(int code). From: FUN_10008f00 (ret 4).
float VirtualPad::merge_value(int code, float stock_value) const noexcept {
    const float vr = vr_analog(tier_for(tiers_, code), code);
    return vr > stock_value ? vr : stock_value;
}

// float GetAxis(int code, bool negate). From: FUN_10008e20 (ret 8).
float VirtualPad::merge_axis(int code, float stock_axis) const noexcept {
    const float vr = vr_analog(tier_for(tiers_, code), code);
    return std::fabs(vr) > std::fabs(stock_axis) ? vr : stock_axis;
}

// const char* ControlToName(uint16 code). From: FUN_10009030 (ret 4).
bool VirtualPad::shows_vr_glyphs() const noexcept {
    return vr_glyphs_ && vr_glyphs_allowed_.load(std::memory_order_relaxed);
}

const char* VirtualPad::vr_glyph(int stock) const noexcept {
    // Bounds checked: "none" or a code outside the game's pad codes shows
    // the unbound glyph (04 §3.3).
    if (stock < 0 || static_cast<std::size_t>(stock) >= glyphs_.size()) return kGlyphUnbound;
    return glyphs_.name(static_cast<std::size_t>(stock));
}

// bool HasActivity(). From: FUN_10008fa0 (ret 0).
ActivityResult VirtualPad::on_activity(bool stock_activity, PlayerDevices devices) noexcept {
    // Read and cleared as one operation, so activity the render thread
    // writes in between is never lost (04 §3.3).
    const bool vr = vr_activity_.exchange(false);
    ActivityResult result{stock_activity || vr, DeviceSwitch::None};
    // Without the mod's UI archive the prompts stay in stock-glyph mode.
    const bool want_vr = vr && vr_glyphs_allowed_.load(std::memory_order_relaxed);

    // Game fields are touched only while the input manager exists (04 §4.2).
    if (devices == PlayerDevices::NoManager) return result;
    if (result.any && vr_glyphs_ != want_vr) {
        vr_glyphs_ = want_vr;
        // Without both devices the mode still changes and the prompts pick
        // it up at the game's next device change.
        if (devices == PlayerDevices::Both) {
            device_pulse_  = true;
            result.devices = DeviceSwitch::PulseKeyboard;
        }
    } else if (device_pulse_) {
        device_pulse_  = false;
        result.devices = DeviceSwitch::RestorePad;
    } else if (devices == PlayerDevices::Both && glyphs_.take_redraw() && shows_vr_glyphs()) {
        // The VR glyph names changed (22 §10.1): the same redraw as a change
        // of glyph mode.
        device_pulse_  = true;
        result.devices = DeviceSwitch::PulseKeyboard;
    }
    return result;
}

void VirtualPad::use_stock_glyphs_only() noexcept {
    if (vr_glyphs_allowed_.exchange(false)) log_.line(kStockGlyphsLog);
}

} // namespace omvr::core::input
