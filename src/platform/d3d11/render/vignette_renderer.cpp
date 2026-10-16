// OpenMotherVR-Provenance: clean
// The comfort vignette and snap-turn fade pass (spec 22 §4.3, §5).

#include "platform/d3d11/render/vignette_renderer.h"

#include "platform/d3d11/handles.h"

#include <d3d11.h>

#include <array>
#include <cstring>
#include <string_view>

namespace omvr::platform::d3d11 {

namespace {

using core::stereo::VignetteConstants;

#include "platform/d3d11/render/shaders/vignette_shaders.inc"

constexpr std::string_view kCreateFailedLog =
    "[stereo] comfort vignette off: creating its {} failed (0x{:08X}); VR not affected";
constexpr std::string_view kNotTargetLog =
    "[stereo] comfort vignette off: the stereo target (format {}) cannot be drawn into";
constexpr std::string_view kOtherTargetLog =
    "[stereo] comfort vignette skipped: the eye texture is not the one configured";

// vignette.hlsl's constants at b0.
constexpr UINT kConstantSlot = 0;
// One full-target triangle strip.
constexpr UINT kStripVertices = 4;
// The pixel shader resources the pass may unbind by binding the target as
// a render target (D3D11 unbinds a resource's views from the inputs when it
// becomes an output); restored after the draw.
constexpr UINT kSavedSrvSlots  = D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT;
constexpr UINT kSavedTargets   = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
constexpr UINT kSavedViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

template <class T>
void release_one(T*& p) noexcept {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

template <class T, std::size_t N>
void release_all(std::array<T*, N>& ps) noexcept {
    for (auto*& p : ps) release_one(p);
}

// A render target view needs a typed format; the game's target may be
// typeless. The pass only scales colours towards black, so UNORM is right
// for the 8-bit families.
DXGI_FORMAT view_format(DXGI_FORMAT f) noexcept {
    switch (f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default: return f;
    }
}

// Multiplies the target by the shader's output: dest * src colour; the
// target's alpha is kept.
D3D11_BLEND_DESC multiply_blend() noexcept {
    D3D11_BLEND_DESC b{};
    auto& rt                 = b.RenderTarget[0];
    rt.BlendEnable           = TRUE;
    rt.SrcBlend              = D3D11_BLEND_ZERO;
    rt.DestBlend             = D3D11_BLEND_SRC_COLOR;
    rt.BlendOp               = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha         = D3D11_BLEND_ZERO;
    rt.DestBlendAlpha        = D3D11_BLEND_ONE;
    rt.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    return b;
}

// The pipeline state the pass changes, taken before and put back after
// (the same requirement as the mirror draw, 03 §8). Each Get adds a
// reference, which the destructor drops. No allocation.
class SavedState final {
public:
    explicit SavedState(ID3D11DeviceContext* ctx) noexcept : ctx_(ctx) {
        ctx->OMGetRenderTargets(kSavedTargets, targets_.data(), &depth_);
        ctx->OMGetBlendState(&blend_, blend_factor_.data(), &sample_mask_);
        ctx->RSGetViewports(&viewport_count_, viewports_.data());
        ctx->RSGetState(&raster_);
        ctx->IAGetInputLayout(&layout_);
        ctx->IAGetPrimitiveTopology(&topology_);
        ctx->VSGetShader(&vs_, nullptr, nullptr);
        ctx->PSGetShader(&ps_, nullptr, nullptr);
        ctx->GSGetShader(&gs_, nullptr, nullptr);
        ctx->HSGetShader(&hs_, nullptr, nullptr);
        ctx->DSGetShader(&ds_, nullptr, nullptr);
        ctx->PSGetConstantBuffers(kConstantSlot, 1, &ps_cb_);
        ctx->PSGetShaderResources(0, kSavedSrvSlots, srvs_.data());
    }

    ~SavedState() {
        ctx_->OMSetRenderTargets(kSavedTargets, targets_.data(), depth_);
        ctx_->OMSetBlendState(blend_, blend_factor_.data(), sample_mask_);
        ctx_->RSSetViewports(viewport_count_, viewports_.data());
        ctx_->RSSetState(raster_);
        ctx_->IASetInputLayout(layout_);
        ctx_->IASetPrimitiveTopology(topology_);
        ctx_->VSSetShader(vs_, nullptr, 0);
        ctx_->PSSetShader(ps_, nullptr, 0);
        ctx_->GSSetShader(gs_, nullptr, 0);
        ctx_->HSSetShader(hs_, nullptr, 0);
        ctx_->DSSetShader(ds_, nullptr, 0);
        ctx_->PSSetConstantBuffers(kConstantSlot, 1, &ps_cb_);
        ctx_->PSSetShaderResources(0, kSavedSrvSlots, srvs_.data());
        release_all(targets_);
        release_all(srvs_);
        release_one(depth_);
        release_one(blend_);
        release_one(raster_);
        release_one(layout_);
        release_one(vs_);
        release_one(ps_);
        release_one(gs_);
        release_one(hs_);
        release_one(ds_);
        release_one(ps_cb_);
    }

    SavedState(const SavedState&)            = delete;
    SavedState& operator=(const SavedState&) = delete;
    SavedState(SavedState&&)                 = delete;
    SavedState& operator=(SavedState&&)      = delete;

private:
    ID3D11DeviceContext* ctx_;
    std::array<ID3D11RenderTargetView*, kSavedTargets> targets_{};
    ID3D11DepthStencilView* depth_ = nullptr;
    ID3D11BlendState* blend_       = nullptr;
    std::array<float, 4> blend_factor_{};
    UINT sample_mask_    = 0;
    UINT viewport_count_ = kSavedViewports;
    std::array<D3D11_VIEWPORT, kSavedViewports> viewports_{};
    ID3D11RasterizerState* raster_     = nullptr;
    ID3D11InputLayout* layout_         = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology_ = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11VertexShader* vs_            = nullptr;
    ID3D11PixelShader* ps_             = nullptr;
    ID3D11GeometryShader* gs_          = nullptr;
    ID3D11HullShader* hs_              = nullptr;
    ID3D11DomainShader* ds_            = nullptr;
    ID3D11Buffer* ps_cb_               = nullptr;
    std::array<ID3D11ShaderResourceView*, kSavedSrvSlots> srvs_{};
};

} // namespace

VignetteRenderer::~VignetteRenderer() { release(); }

bool VignetteRenderer::ready() const noexcept { return vs_ && ps_ && cb_ && blend_ && rtv_; }

void VignetteRenderer::release() noexcept {
    release_one(vs_);
    release_one(ps_);
    release_one(cb_);
    release_one(blend_);
    release_one(rtv_);
    target_ = {};
}

void VignetteRenderer::configure(core::vr::GpuDevice gpu_device, core::vr::GpuTexture target) noexcept {
    release();
    ID3D11Device* device     = to_d3d(gpu_device);
    ID3D11Texture2D* texture = to_d3d(target);
    if (!device || !texture) return;
    D3D11_TEXTURE2D_DESC td{};
    texture->GetDesc(&td);
    if ((td.BindFlags & D3D11_BIND_RENDER_TARGET) == 0) {
        core::diag::ports::logf(log_, kNotTargetLog, static_cast<int>(td.Format));
        return;
    }
    std::string_view what = "vertex shader";
    HRESULT hr            = device->CreateVertexShader(kVignetteVS, sizeof(kVignetteVS), nullptr, &vs_);
    if (SUCCEEDED(hr)) {
        what = "pixel shader";
        hr   = device->CreatePixelShader(kVignettePS, sizeof(kVignettePS), nullptr, &ps_);
    }
    if (SUCCEEDED(hr)) {
        what = "constant buffer";
        const D3D11_BUFFER_DESC cb{sizeof(VignetteConstants),
                                   D3D11_USAGE_DYNAMIC,
                                   D3D11_BIND_CONSTANT_BUFFER,
                                   D3D11_CPU_ACCESS_WRITE,
                                   0,
                                   0};
        hr = device->CreateBuffer(&cb, nullptr, &cb_);
    }
    if (SUCCEEDED(hr)) {
        what                     = "blend state";
        const D3D11_BLEND_DESC b = multiply_blend();
        hr                       = device->CreateBlendState(&b, &blend_);
    }
    if (SUCCEEDED(hr)) {
        what = "target view";
        D3D11_RENDER_TARGET_VIEW_DESC rv{};
        rv.Format = view_format(td.Format);
        rv.ViewDimension =
            td.SampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
        hr = device->CreateRenderTargetView(texture, &rv, &rtv_);
    }
    if (FAILED(hr) || !ready()) {
        release();
        core::diag::ports::logf(log_, kCreateFailedLog, what, static_cast<unsigned long>(hr));
        return;
    }
    target_ = target;
    width_  = static_cast<float>(td.Width);
    height_ = static_cast<float>(td.Height);
}

void VignetteRenderer::draw(core::vr::GpuContext context, core::vr::GpuTexture target,
                            const VignetteConstants& constants) noexcept {
    if (!ready()) return;
    if (target != target_) {
        if (other_target_logged_.first()) log_.line(kOtherTargetLog);
        return;
    }
    ID3D11DeviceContext* ctx = to_d3d(context);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(ctx->Map(cb_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    std::memcpy(mapped.pData, &constants, sizeof(constants));
    ctx->Unmap(cb_, 0);

    const SavedState saved(ctx);
    ctx->OMSetRenderTargets(1, &rtv_, nullptr);
    ctx->OMSetBlendState(blend_, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
    const D3D11_VIEWPORT vp{0.0f, 0.0f, width_, height_, 0.0f, 1.0f};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(nullptr);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->VSSetShader(vs_, nullptr, 0);
    ctx->PSSetShader(ps_, nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->HSSetShader(nullptr, nullptr, 0);
    ctx->DSSetShader(nullptr, nullptr, 0);
    ctx->PSSetConstantBuffers(kConstantSlot, 1, &cb_);
    ctx->Draw(kStripVertices, 0);
}

} // namespace omvr::platform::d3d11
