#include "game/detour.h"

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "platform/win32/patching/inline_hook.h"

#include <cstring>

namespace omvr::game {

namespace diag = core::diag;

bool detour(std::uint8_t* target, const void* replacement, std::span<const std::uint8_t> expected,
            void** original, const char* name) noexcept {
    if (!target || !original || std::memcmp(target, expected.data(), expected.size()) != 0) {
        diag::logf("[hook] {}: unexpected code at {}, not detoured", name, static_cast<void*>(target));
        return false;
    }
    return platform::win32::install_inline_hook(target, replacement, original, name);
}

} // namespace omvr::game
