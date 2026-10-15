#pragma once

#include "core/stereo/types.h"

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11PixelShader;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;
struct ID3D11VertexShader;

// The desktop mirror (mirror.hlsl): one shader for every mode; the frame
// kind picks the constant buffer.
namespace omvr::platform::d3d11 {

// The mirror's D3D11 objects, kept where the caller stores them (the stereo
// device's own region, +0x21C..+0x22F).
struct MirrorObjects {
    ID3D11VertexShader*& vs;
    ID3D11PixelShader*&  ps;
    ID3D11Buffer*&       cb_screen;  // screen frames
    ID3D11Buffer*&       cb;         // head-tracked frames
    ID3D11SamplerState*& sampler;
};

void release_mirror(const MirrorObjects& m) noexcept;
bool mirror_ready(const MirrorObjects& m) noexcept;

// (Re)creates the mirror's shaders, constants and sampler. On a failure
// none is kept and the window is cleared to black instead (end frame); VR is
// unaffected. Slot 3 only, so not per frame.
void create_mirror(const MirrorObjects& m, ID3D11Device* device, const core::stereo::MirrorConstants& eyes,
                   const core::stereo::MirrorConstants& screen) noexcept;

// EndFrame: draws the stereo texture `source` into the window's back buffer,
// the eyes while the game tracks the head, else the virtual screen's region.
// LibOVR's EndFrame presented in SDK-distortion mode, so the game does not.
void draw_mirror(const MirrorObjects& m, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* const* source,
                 bool head_tracked) noexcept;

} // namespace omvr::platform::d3d11
