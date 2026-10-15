#pragma once

// DXGI and D3D11 values their headers do not name.
namespace omvr::platform::d3d11 {

// IDXGISwapChain vtable layout (from MSDN). The stereo path composes its
// frame in Present, and the eye buffers have to follow game resolution
// changes (ResizeBuffers).
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
inline constexpr unsigned kPresentVtblIndex       = 8;
inline constexpr unsigned kResizeBuffersVtblIndex = 13;
inline constexpr unsigned kSetFullscreenStateVtblIndex = 10;

// ID3D11Device: 15 CreatePixelShader (hooked for the shader fixes).
inline constexpr unsigned kCreatePixelShaderVtblIndex = 15;

// The system DLL the proxy forwards to, under the system directory.
inline constexpr wchar_t kSystemD3d11[] = L"\\d3d11.dll";

} // namespace omvr::platform::d3d11
