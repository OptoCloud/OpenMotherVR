// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/stereo/types.h"
#include "core/vr/types.h"
#include "platform/win32/com_ptr.h"
#include "util/sync/once_latch.h"

struct ID3D11BlendState;
struct ID3D11Buffer;
struct ID3D11PixelShader;
struct ID3D11RenderTargetView;
struct ID3D11VertexShader;

// The comfort vignette and snap-turn fade pass (spec 22 §4.3, §5,
// vignette.hlsl): darkens each eye half of the stereo target in place, in
// slot 6 before the frame goes to the runtime, so OpenVR's submit, the
// OpenXR copy and the desktop mirror all carry it.
namespace omvr::platform::d3d11 {

class VignetteRenderer final {
public:
    // Both defined where the D3D11 types are complete (they release pass_).
    explicit VignetteRenderer(core::diag::ports::Log& log) noexcept;
    ~VignetteRenderer();
    VignetteRenderer(const VignetteRenderer&)            = delete;
    VignetteRenderer& operator=(const VignetteRenderer&) = delete;
    VignetteRenderer(VignetteRenderer&&)                 = delete;
    VignetteRenderer& operator=(VignetteRenderer&&)      = delete;

    // Slot 3 (the game (re)creates its stereo target), render thread:
    // (re)creates the shaders, blend state, constants and a render target
    // view of `target`. On a failure nothing is kept and the pass is off
    // until the next call; VR is unaffected.
    void configure(core::vr::GpuDevice device, core::vr::GpuTexture target) noexcept;

    // Slot 6, render thread: darkens `target` by `constants`. Only called
    // while there is something to draw. Leaves the pipeline state it changes
    // as it found it. Per frame: no allocation, no wait, at most one log
    // line ever.
    void draw(core::vr::GpuContext context, core::vr::GpuTexture target,
              const core::stereo::VignetteConstants& constants) noexcept;

private:
    // The pass's objects: all of them, or none.
    struct Pass {
        win32::ComPtr<ID3D11VertexShader> vs;
        win32::ComPtr<ID3D11PixelShader> ps;
        win32::ComPtr<ID3D11Buffer> cb;
        win32::ComPtr<ID3D11BlendState> blend;
        win32::ComPtr<ID3D11RenderTargetView> rtv;
        bool ready() const noexcept { return vs && ps && cb && blend && rtv; }
    };

    core::diag::ports::Log& log_;
    Pass pass_;
    core::vr::GpuTexture target_ = {};
    float width_                 = 0.0f;
    float height_                = 0.0f;
    util::OnceLatch other_target_logged_;
};

} // namespace omvr::platform::d3d11
