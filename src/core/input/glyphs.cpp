// From: FUN_10001090 (glyph names). Analysis: dumps/06d_vr_input.md.

#include "core/input/glyphs.h"

namespace omvr::core::input {

// Constructed before any thread reads the names: plain relaxed stores.
Glyphs::Glyphs() noexcept {
    for (std::size_t i = 0; i < names_.size(); ++i) names_[i].store(kStockPadNames[i], std::memory_order_relaxed);
}

void Glyphs::set_glyph(int pad_code, const char* name) noexcept {
    if (name && pad_code >= 0 && static_cast<std::size_t>(pad_code) < names_.size()) {
        names_[static_cast<std::size_t>(pad_code)].store(name, std::memory_order_relaxed);
    }
}

} // namespace omvr::core::input
