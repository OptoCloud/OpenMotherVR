#pragma once

#include "game/addresses.h"

#include <cstdint>

// The game's renderer accessors (plain __cdecl in AI.exe).
namespace omvr::game::stereo {

// The game's Renderer, null before it exists.
inline void* renderer() noexcept {
    return *reinterpret_cast<void**>(ai_address(AiAddr::RendererPtr));
}

// One of the renderer's D3D11 objects (device, context, swapchain).
template <class T>
T* renderer_get(AiAddr slot) noexcept {
    using Fn = T* (__cdecl*)(void*);
    return reinterpret_cast<Fn>(ai_address(slot))(renderer());
}

// A render target's texture or view by its index.
template <class T>
T* renderer_rt(AiAddr slot, std::uint16_t index) noexcept {
    using Fn = T* (__cdecl*)(void*, std::uint32_t);
    return reinterpret_cast<Fn>(ai_address(slot))(renderer(), index);
}

// The texture index of a stereo render target (its +0x80 field).
inline std::uint16_t render_target_index(void* stereo_rt) noexcept {
    return *reinterpret_cast<std::uint16_t*>(static_cast<std::uint8_t*>(stereo_rt) + 0x80);
}

} // namespace omvr::game::stereo
