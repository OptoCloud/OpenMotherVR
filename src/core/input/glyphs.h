#pragma once

#include "game/constants.h"

#include <array>
#include <atomic>
#include <cstddef>

namespace omvr::core::input {

// The game's own pad control names, by pad code (04 §2.3): what the stock
// ControlToName returns, and the glyphs shown until a VR set is chosen.
inline constexpr std::array<const char*, game::kPadCodeCount> kStockPadNames = {
    "A",    "B",     "X",        "Y",        "DPAD_UP",   "DPAD_DOWN", "DPAD_LEFT",     "DPAD_RIGHT",
    "BACK", "START", "LB",       "RB",       "LT_BUTTON", "RT_BUTTON", "LSTICK_BUTTON", "RSTICK_BUTTON",
    "LT",   "RT",    "LSTICK_X", "LSTICK_Y", "RSTICK_X",  "RSTICK_Y",  "TOUCH_ZOOM_IN", "TOUCH_ZOOM_OUT",
};

// The button-prompt glyph shown for each pad code while VR glyphs are
// active. Initially the game's pad names; the runtimes override some with
// "VR_*" ids that the modded UI maps to controller images (04 §4.1).
//
// Original: the std::vector<std::string> at DAT_100686d4, built by
// FUN_10001090. Literals here, so the pointers handed to the game stay valid
// whatever set_glyph does later. Atomic: the render thread picks the glyph
// set (set_glyph) while the game thread reads names, and a reader must get
// one whole pointer, the old or the new (spec 04 section 3.6). The strings
// themselves never change, so relaxed ordering is enough.
class Glyphs {
public:
    Glyphs() noexcept;

    // The game keeps the pointer it is handed, so `name` must be a string
    // literal (or otherwise live for the whole process). Render thread.
    // True when the name changed.
    bool set_glyph(int pad_code, const char* name) noexcept;

    // The names changed while prompts may be on screen: the pad's activity
    // hook makes the game redraw them (04 §4.2, 22 §10.1). Render thread
    // requests, game thread takes.
    void request_redraw() noexcept { redraw_.store(true, std::memory_order_relaxed); }
    [[nodiscard]] bool take_redraw() const noexcept {
        return redraw_.exchange(false, std::memory_order_relaxed);
    }

    // The glyph for pad code `code`, which must be in range. Game thread.
    [[nodiscard]] const char* name(std::size_t code) const noexcept {
        return names_[code].load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::size_t size() const noexcept { return names_.size(); }

private:
    std::array<std::atomic<const char*>, game::kPadCodeCount> names_;
    // Mutable: taking the request is not a change of the names.
    mutable std::atomic<bool> redraw_{false};
};

} // namespace omvr::core::input
