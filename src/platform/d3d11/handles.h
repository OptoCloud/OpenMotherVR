#pragma once

#include "core/vr/types.h"

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;

// The D3D11 interfaces behind core's opaque GPU handles.
namespace omvr::platform::d3d11 {

inline ID3D11Device* to_d3d(core::vr::GpuDevice h) noexcept {
    return reinterpret_cast<ID3D11Device*>(static_cast<std::uintptr_t>(h));
}
inline ID3D11DeviceContext* to_d3d(core::vr::GpuContext h) noexcept {
    return reinterpret_cast<ID3D11DeviceContext*>(static_cast<std::uintptr_t>(h));
}
inline ID3D11Texture2D* to_d3d(core::vr::GpuTexture h) noexcept {
    return reinterpret_cast<ID3D11Texture2D*>(static_cast<std::uintptr_t>(h));
}

inline core::vr::GpuDevice gpu_device(ID3D11Device* p) noexcept {
    return core::vr::GpuDevice{reinterpret_cast<std::uintptr_t>(p)};
}
inline core::vr::GpuContext gpu_context(ID3D11DeviceContext* p) noexcept {
    return core::vr::GpuContext{reinterpret_cast<std::uintptr_t>(p)};
}
inline core::vr::GpuTexture gpu_texture(ID3D11Texture2D* p) noexcept {
    return core::vr::GpuTexture{reinterpret_cast<std::uintptr_t>(p)};
}

} // namespace omvr::platform::d3d11
