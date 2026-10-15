#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace omvr::core::input {

// The button-prompt glyph shown for each pad code while VR glyphs are
// active. Initially the game's pad names ("A", "B", ...); the runtimes
// override some with "VR_*" ids that the modded UI maps to controller
// images.
//
// Original: the std::vector<std::string> at DAT_100686d4, built by
// FUN_10001090. Literals here, so the pointers handed to the game stay valid
// whatever set_glyph does later. Atomic: the render thread picks the glyph
// set (set_glyph) while the game thread reads names, and a reader must get
// one whole pointer, the old or the new (spec 04 section 3.6). The strings
// themselves never change, so relaxed ordering is enough.
class Glyphs {
public:
    // The game keeps the pointer it is handed, so `name` must be a string
    // literal (or otherwise live for the whole process).
    void set_glyph(int pad_code, const char* name) noexcept;

    // The glyph for pad code `code`, which must be in range.
    const char* name(std::size_t code) const noexcept { return g_glyphs[code].load(std::memory_order_relaxed); }
    std::size_t size() const noexcept { return g_glyphs.size(); }

private:
    std::array<std::atomic<const char*>, 24> g_glyphs = {
        "A", "B", "X", "Y", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
        "BACK", "START", "LB", "RB", "LT_BUTTON", "RT_BUTTON", "LSTICK_BUTTON",
        "RSTICK_BUTTON", "LT", "RT", "LSTICK_X", "LSTICK_Y", "RSTICK_X", "RSTICK_Y",
        "TOUCH_ZOOM_IN", "TOUCH_ZOOM_OUT",
    };
};

} // namespace omvr::core::input
