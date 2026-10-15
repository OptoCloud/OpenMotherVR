#include "game/event_tap.h"

#include "game/code_address.h"
#include "game/detour.h"
#include "game/x86.h"
#include "platform/win32/module.h"
#include "platform/win32/patching/pe_image.h"

#include <array>
#include <cstdint>

namespace omvr::game {

namespace {

// `mov eax, ecx; ret 4`: the tap's whole body.
constexpr std::uint8_t kModRmEaxEcx            = 0xC1;
constexpr std::uint8_t kRetImm16               = 0xC2;
constexpr std::uint8_t kArgBytes               = 4; // one stack argument, popped by the callee
constexpr std::array<std::uint8_t, 5> kTapNoOp = {x86::kMovR32Rm32, kModRmEaxEcx, kRetImm16, kArgBytes, 0x00};

// The bounds of the game's image, read once: the main module stays mapped
// for the life of the process.
struct ImageBounds {
    std::uint32_t lo;
    std::uint32_t hi;
};

ImageBounds game_image_bounds() noexcept {
    const auto* base = static_cast<const std::uint8_t*>(platform::win32::module_handle(nullptr));
    return {address_of(base), address_of(base) + platform::win32::image_size_of(base)};
}

} // namespace

bool in_game_image(const char* p) noexcept {
    static const ImageBounds bounds = game_image_bounds();
    const std::uint32_t a           = address_of(p);
    return a >= bounds.lo && a < bounds.hi;
}

bool install_event_tap(std::uint8_t* at, TapFn replacement, TapFn& trampoline,
                       core::diag::ports::Log& log) noexcept {
    return detour(at, replacement, kTapNoOp, trampoline, "game event tap", log);
}

} // namespace omvr::game
