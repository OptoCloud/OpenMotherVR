// The desktop mirror. From: FUN_1001eda0 (the original's mirror).
// Analysis: dumps/06a_stereo_interface.md.

#include "platform/d3d11/render/mirror_renderer.h"

#include "core/diag/logger.h" // R: render (A3): log through an injected Log&

#include <d3d11.h>

#include <utility>

namespace omvr::platform::d3d11 {

namespace {

namespace diag = core::diag;
using core::stereo::MirrorConstants;

#include "platform/d3d11/render/shaders/mirror_shaders.inc"

template <class T>
void release(T*& p) noexcept {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

} // namespace

void release_mirror(const MirrorObjects& m) noexcept {
    release(m.vs);
    release(m.ps);
    release(m.cb_screen);
    release(m.cb);
    release(m.sampler);
}

bool mirror_ready(const MirrorObjects& m) noexcept {
    return m.vs && m.ps && m.cb_screen && m.cb && m.sampler;
}

void create_mirror(const MirrorObjects& m, ID3D11Device* device, const MirrorConstants& eyes,
                   const MirrorConstants& screen) noexcept {
    release_mirror(m);
    const char* what = "vertex shader";
    HRESULT hr = device->CreateVertexShader(kMirrorVS, sizeof(kMirrorVS), nullptr, &m.vs);
    if (SUCCEEDED(hr)) {
        what = "pixel shader";
        hr = device->CreatePixelShader(kMirrorPS, sizeof(kMirrorPS), nullptr, &m.ps);
    }
    const D3D11_BUFFER_DESC cb_desc{sizeof(MirrorConstants), D3D11_USAGE_DEFAULT,
                                    D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
    if (SUCCEEDED(hr)) {
        what = "constant buffer";
        // The eyes' constants: the caller's (core::stereo::mirror_constants).
        const D3D11_SUBRESOURCE_DATA data{&eyes, 0, 0};
        hr = device->CreateBuffer(&cb_desc, &data, &m.cb);
    }
    if (SUCCEEDED(hr)) {
        // The screen's constants: the caller's.
        const D3D11_SUBRESOURCE_DATA data{&screen, 0, 0};
        hr = device->CreateBuffer(&cb_desc, &data, &m.cb_screen);
    }
    if (SUCCEEDED(hr)) {
        what = "sampler";
        D3D11_SAMPLER_DESC s{};
        s.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        s.MaxAnisotropy  = 1;
        s.ComparisonFunc = D3D11_COMPARISON_NEVER;
        s.MinLOD         = -D3D11_FLOAT32_MAX;
        s.MaxLOD         = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&s, &m.sampler);
    }
    if (FAILED(hr) || !mirror_ready(m)) {
        release_mirror(m);
        diag::logf("[stereo] desktop mirror off: creating its {} failed (0x{:08X}); the window shows black, "
                   "VR not affected", what, static_cast<unsigned long>(hr));
    }
}

void draw_mirror(const MirrorObjects& m, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* const* source,
                 bool head_tracked) noexcept {
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) {
        // Nothing to draw into; the window keeps its last picture.
        static bool s_logged = false;
        if (!std::exchange(s_logged, true)) {
            diag::log_line("[stereo] desktop mirror: the game has no output bound at end of frame");
        }
        return;
    }
    if (!mirror_ready(m)) {
        const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        ctx->ClearRenderTargetView(rtv, black);
        rtv->Release();
        return;
    }
    const float dark_red[4] = {0.6f, 0.0f, 0.0f, 1.0f};
    ctx->ClearRenderTargetView(rtv, dark_red);
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    // Draw over the whole window, not whatever viewport the game left.
    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    if (ID3D11Texture2D* tex = nullptr;
        res && SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex)))) {
        D3D11_TEXTURE2D_DESC td{};
        tex->GetDesc(&td);
        const D3D11_VIEWPORT vp{0.0f, 0.0f, float(td.Width), float(td.Height), 0.0f, 1.0f};
        ctx->RSSetViewports(1, &vp);
        tex->Release();
    }
    if (res) res->Release();
    ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    ctx->RSSetState(nullptr);
    ctx->IASetInputLayout(nullptr);
    ctx->VSSetShader(m.vs, nullptr, 0);
    ctx->PSSetShader(m.ps, nullptr, 0);
    ctx->PSSetShaderResources(1, 1, source);
    // The eyes while the game tracks the head, else the virtual screen's
    // region (menus, videos, loading).
    ctx->PSSetConstantBuffers(0, 1, head_tracked ? &m.cb : &m.cb_screen);
    ctx->PSSetSamplers(0, 1, &m.sampler);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);
    rtv->Release();  // the original leaks this reference every frame
}

} // namespace omvr::platform::d3d11
