#pragma once

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/stereo/field.h"

#include <cstddef>
#include <cstdint>

// The game's renderer accessors (plain __cdecl in AI.exe, 03 §12).
namespace omvr::game::stereo {

// The 16-bit renderer render-target index at +0x80 of the stereo render
// target object the game passes to device slot 3 (02 §3.2, §8).
inline constexpr std::ptrdiff_t kStereoRtTextureIndex = 0x80;

// The game's Renderer, null before it exists.
inline void* renderer(const AddressMap& addresses) noexcept {
    return pointer_at(addresses.at(AiAddr::RendererPtr));
}

// One of the renderer's D3D11 objects (device, context, swapchain).
template <class T>
T* renderer_get(const AddressMap& addresses, AiAddr slot) noexcept {
    using Fn     = T*(__cdecl*)(void*);
    const Fn get = CodeAddress{addresses.at(slot)};
    return get(renderer(addresses));
}

// A render target's texture or view by its index.
template <class T>
T* renderer_rt(const AddressMap& addresses, AiAddr slot, std::uint16_t index) noexcept {
    using Fn     = T*(__cdecl*)(void*, std::uint32_t);
    const Fn get = CodeAddress{addresses.at(slot)};
    return get(renderer(addresses), index);
}

// The texture index of the game's stereo render target `stereo_rt`.
inline std::uint16_t render_target_index(void* stereo_rt) noexcept {
    return field<std::uint16_t>(stereo_rt, kStereoRtTextureIndex);
}

} // namespace omvr::game::stereo
