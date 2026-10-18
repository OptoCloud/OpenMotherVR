// The desktop mirror (03 §8). From: FUN_1001eda0 (the original's mirror).
// Analysis: dumps/06a_stereo_interface.md.

#include "platform/d3d11/render/mirror_renderer.h"

#include "core/stereo/tuning.h"
#include "platform/d3d11/handles.h"
#include "platform/win32/com_ptr.h"

#include <d3d11.h>

#include <string_view>

namespace omvr::platform::d3d11 {

namespace {

using core::stereo::MirrorConstants;
using win32::ComPtr;
using win32::release_one;

#include "platform/d3d11/render/shaders/mirror_shaders.inc"

constexpr std::string_view kNoOutputLog =
    "[stereo] desktop mirror: the game has no output bound at end of frame";
constexpr std::string_view kCreateFailedLog =
    "[stereo] desktop mirror off: creating its {} failed (0x{:08X}); the window shows black, VR not affected";

// mirror.hlsl's bindings: the stereo texture at t1, where the original's
// mirror bound it (slot 0 is left as the game set it), the constants at b0
// and the sampler at s0.
constexpr UINT kSourceSlot   = 1;
constexpr UINT kConstantSlot = 0;
constexpr UINT kSamplerSlot  = 0;
// One full-window triangle strip.
constexpr UINT kStripVertices = 4;

D3D11_SAMPLER_DESC bilinear_clamp() noexcept {
    D3D11_SAMPLER_DESC s{};
    s.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    s.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    s.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    s.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    s.MaxAnisotropy  = 1;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER;
    s.MinLOD         = -D3D11_FLOAT32_MAX;
    s.MaxLOD         = D3D11_FLOAT32_MAX;
    return s;
}

// Draw over the whole window, not whatever viewport the game left.
void cover_target(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv) noexcept {
    ID3D11Resource* raw_res = nullptr;
    rtv->GetResource(&raw_res);
    const auto res           = ComPtr<ID3D11Resource>::adopt(raw_res);
    ID3D11Texture2D* raw_tex = nullptr;
    if (!res || FAILED(res->QueryInterface(IID_PPV_ARGS(&raw_tex)))) return;
    const auto tex = ComPtr<ID3D11Texture2D>::adopt(raw_tex);
    D3D11_TEXTURE2D_DESC td{};
    tex->GetDesc(&td);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(td.Width), static_cast<float>(td.Height),
                            0.0f, 1.0f};
    ctx->RSSetViewports(1, &vp);
}

} // namespace

bool MirrorRenderer::ready() const noexcept {
    return objects_.vs && objects_.ps && objects_.cb_screen && objects_.cb && objects_.sampler;
}

void MirrorRenderer::release() noexcept {
    release_one(objects_.vs);
    release_one(objects_.ps);
    release_one(objects_.cb_screen);
    release_one(objects_.cb);
    release_one(objects_.sampler);
}

void MirrorRenderer::create(core::vr::GpuDevice gpu_device, const MirrorConstants& eyes,
                            const MirrorConstants& screen) noexcept {
    ID3D11Device* device = to_d3d(gpu_device);
    release();
    std::string_view what = "vertex shader";
    HRESULT hr            = device->CreateVertexShader(kMirrorVS, sizeof(kMirrorVS), nullptr, &objects_.vs);
    if (SUCCEEDED(hr)) {
        what = "pixel shader";
        hr   = device->CreatePixelShader(kMirrorPS, sizeof(kMirrorPS), nullptr, &objects_.ps);
    }
    const D3D11_BUFFER_DESC cb_desc{
        sizeof(MirrorConstants), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
    if (SUCCEEDED(hr)) {
        what = "constant buffer";
        // The eyes' constants: the caller's (core::stereo::mirror_constants).
        const D3D11_SUBRESOURCE_DATA data{&eyes, 0, 0};
        hr = device->CreateBuffer(&cb_desc, &data, &objects_.cb);
    }
    if (SUCCEEDED(hr)) {
        // The screen's constants: the caller's.
        const D3D11_SUBRESOURCE_DATA data{&screen, 0, 0};
        hr = device->CreateBuffer(&cb_desc, &data, &objects_.cb_screen);
    }
    if (SUCCEEDED(hr)) {
        what                       = "sampler";
        const D3D11_SAMPLER_DESC s = bilinear_clamp();
        hr                         = device->CreateSamplerState(&s, &objects_.sampler);
    }
    if (FAILED(hr) || !ready()) {
        release();
        core::diag::ports::logf(log_, kCreateFailedLog, what, static_cast<unsigned long>(hr));
    }
}

void MirrorRenderer::draw(core::vr::GpuContext context, bool head_tracked) noexcept {
    ID3D11DeviceContext* ctx        = to_d3d(context);
    ID3D11RenderTargetView* raw_rtv = nullptr;
    ctx->OMGetRenderTargets(1, &raw_rtv, nullptr);
    const auto rtv = ComPtr<ID3D11RenderTargetView>::adopt(raw_rtv);
    if (!rtv) {
        // Nothing to draw into; the window keeps its last picture.
        if (no_output_logged_.first()) log_.line(kNoOutputLog);
        return;
    }
    if (!ready()) {
        ctx->ClearRenderTargetView(rtv.get(), core::stereo::kMirrorOffColour.data());
        return;
    }
    ctx->ClearRenderTargetView(rtv.get(), core::stereo::kMirrorClearColour.data());
    ID3D11RenderTargetView* const target = rtv.get();
    ctx->OMSetRenderTargets(1, &target, nullptr);
    cover_target(ctx, target);
    ctx->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
    ctx->RSSetState(nullptr);
    ctx->IASetInputLayout(nullptr);
    ctx->VSSetShader(objects_.vs, nullptr, 0);
    ctx->PSSetShader(objects_.ps, nullptr, 0);
    ctx->PSSetShaderResources(kSourceSlot, 1, source_);
    // The eyes while the game tracks the head, else the virtual screen's
    // region (menus, videos, loading).
    ctx->PSSetConstantBuffers(kConstantSlot, 1, head_tracked ? &objects_.cb : &objects_.cb_screen);
    ctx->PSSetSamplers(kSamplerSlot, 1, &objects_.sampler);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(kStripVertices, 0);
    // rtv releases the reference OMGetRenderTargets added (the original
    // leaked it every frame).
}

} // namespace omvr::platform::d3d11
