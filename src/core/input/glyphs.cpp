// From: FUN_10001090 (glyph names). Analysis: dumps/06d_vr_input.md.

#include "core/input/glyphs.h"

namespace omvr::core::input {

void Glyphs::set_glyph(int pad_code, const char* name) noexcept {
    if (name && pad_code >= 0 && static_cast<std::size_t>(pad_code) < g_glyphs.size()) {
        g_glyphs[static_cast<std::size_t>(pad_code)].store(name, std::memory_order_relaxed);
    }
}

} // namespace omvr::core::input
