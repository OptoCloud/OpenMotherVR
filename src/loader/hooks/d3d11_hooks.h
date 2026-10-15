// OpenMotherVR-Provenance: clean
#pragma once

namespace omvr::loader::hooks {

// Hooks ID3D11Device::CreatePixelShader (the shader fixes) on the first
// device the game creates. Shared by every device of the class; the fix only
// rewrites the game's own dither pattern, so another device's shaders pass
// through unchanged.
void install_device_hooks(void* device) noexcept;

// Wires up the swapchain hooks (Present, SetFullscreenState, ResizeBuffers)
// and logs a one-time GetDesc snapshot of the chain (resolution, format,
// buffer count). Invoked by the exported D3D11CreateDeviceAndSwapChain once
// it succeeds. `swapchain` becomes the game's swapchain, the only one the
// hooks act on; the vtable is patched once.
void install_swapchain_hooks(void* swapchain) noexcept;

} // namespace omvr::loader::hooks
