// OpenMotherVR-Provenance: clean
#pragma once

#include <cstddef>

// DXGI and D3D11 values their headers do not name.
namespace omvr::platform::d3d11 {

// IDXGISwapChain vtable layout (dxgi.h declaration order). The stereo path
// composes its frame in Present, and the eye buffers have to follow game
// resolution changes (ResizeBuffers).
//   0..2  IUnknown                QueryInterface, AddRef, Release
//   3..6  IDXGIObject             Set*/GetPrivateData, GetParent
//   7     IDXGIDeviceSubObject    GetDevice
//   8     IDXGISwapChain          Present                  <-- hooked
//   9                             GetBuffer
//   10                            SetFullscreenState       <-- hooked
//   11                            GetFullscreenState
//   12                            GetDesc                  <-- called once
//   13                            ResizeBuffers            <-- hooked
//   14                            ResizeTarget
//   ...
inline constexpr std::size_t kPresentVtblIndex            = 8;
inline constexpr std::size_t kSetFullscreenStateVtblIndex = 10;
inline constexpr std::size_t kResizeBuffersVtblIndex      = 13;

// ID3D11Device: 15 CreatePixelShader (hooked for the shader fixes, 08).
inline constexpr std::size_t kCreatePixelShaderVtblIndex = 15;
// ID3D11Device: 12 CreateVertexShader (hooked for the upright sprites, 24 §14).
inline constexpr std::size_t kCreateVertexShaderVtblIndex = 12;

// The system DLL the proxy forwards to, under the system directory (01 §1.1).
inline constexpr wchar_t kSystemD3d11[] = L"\\d3d11.dll";

// The export AI.exe imports, which the proxy intercepts (01 §1.3).
inline constexpr const char* kCreateDeviceAndSwapChainExport = "D3D11CreateDeviceAndSwapChain";

} // namespace omvr::platform::d3d11
