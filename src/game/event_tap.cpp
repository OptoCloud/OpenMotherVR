#include "game/event_tap.h"

#include "game/detour.h"
#include "platform/win32/module.h"
#include "platform/win32/patching/pe_image.h"

#include <utility>

namespace omvr::game {

bool in_game_image(const char* p) noexcept {
    static const auto [lo, hi] = [] {
        const auto* base = static_cast<const std::uint8_t*>(platform::win32::module_handle(nullptr));
        return std::pair{reinterpret_cast<std::uintptr_t>(base),
                         reinterpret_cast<std::uintptr_t>(base) + platform::win32::image_size_of(base)};
    }();
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    return a >= lo && a < hi;
}

bool install_event_tap(std::uint8_t* at, TapFn replacement, TapFn& original) noexcept {
    static constexpr std::uint8_t kNoOp[] = {0x8B, 0xC1, 0xC2, 0x04, 0x00};   // mov eax, ecx; ret 4
    return detour(at, replacement, kNoOp, original, "game event tap");
}

} // namespace omvr::game
