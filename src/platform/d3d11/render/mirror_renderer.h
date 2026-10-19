#pragma once

#include "core/diag/ports/log.h"
#include "core/stereo/ports/desktop_mirror.h"
#include "core/stereo/types.h"
#include "util/sync/once_latch.h"

struct ID3D11Buffer;
struct ID3D11DeviceContext;
struct ID3D11PixelShader;
struct ID3D11RenderTargetView;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;
struct ID3D11VertexShader;
struct IDXGISwapChain;

// The desktop mirror (03 §8, mirror.hlsl): one shader for every mode; the
// frame kind picks the constant buffer.
namespace omvr::platform::d3d11 {

// The mirror's D3D11 objects, kept where the caller stores them (the stereo
// device's own region, +0x21C..+0x22F): raw pointers in the game's layout,
// owned by the mirror and released only by it.
struct MirrorObjects {
    ID3D11VertexShader*& vs;
    ID3D11PixelShader*& ps;
    ID3D11Buffer*& cb_screen; // screen frames
    ID3D11Buffer*& cb;        // head-tracked frames
    ID3D11SamplerState*& sampler;
};

// The mirror on the stereo device's objects, drawing from `source` (the eye
// texture's view, read at each draw). Render thread.
class MirrorRenderer final : public core::stereo::ports::DesktopMirror {
public:
    MirrorRenderer(const MirrorObjects& objects, ID3D11ShaderResourceView* const* source,
                   core::diag::ports::Log& log) noexcept
        : objects_(objects), source_(source), log_(log) {}

    // (Re)creates the mirror's shaders, constants and sampler. On a failure
    // none is kept and the window is cleared to black instead (draw); VR is
    // unaffected. Slot 3 only, so not per frame.
    void create(core::vr::GpuDevice device, const core::stereo::MirrorConstants& eyes,
                const core::stereo::MirrorConstants& screen) noexcept override;

    // EndFrame: draws the stereo texture into the window's back buffer, the
    // eyes while the game tracks the head, else the virtual screen's region.
    // LibOVR's EndFrame presented in SDK-distortion mode, so the game does
    // not. Per frame: no allocation, one log line at most once.
    void draw(core::vr::GpuContext context, bool head_tracked) noexcept override;

    // The game's swapchain (the most recent one), whose back buffer the
    // mirror binds itself when the game ends a frame with nothing bound for
    // output (03 §8.3, Target). Not a reference of its own: the game owns
    // it. A different swapchain drops the view of the old one's buffer.
    // Render thread (swapchain creation).
    void use_swapchain(IDXGISwapChain* swapchain) noexcept;

    // Drops the view of the back buffer: before every ResizeBuffers, which
    // fails while any reference to the buffers is held (03 §8.3). The next
    // frame that needs it makes a new one. Render thread.
    void release_back_buffer() noexcept;

private:
    bool ready() const noexcept;
    void release() noexcept;
    // The kept view of the back buffer, made on the first frame that needs
    // it after a swapchain change or a resize, not per frame. Null when the
    // mirror has no swapchain or the view cannot be made.
    ID3D11RenderTargetView* back_buffer_view(ID3D11DeviceContext* ctx) noexcept;

    MirrorObjects objects_;
    ID3D11ShaderResourceView* const* source_;
    core::diag::ports::Log& log_;
    util::OnceLatch no_output_logged_;
    IDXGISwapChain* swapchain_ = nullptr;
    // Owned (one reference), released by release_back_buffer.
    ID3D11RenderTargetView* back_buffer_rtv_ = nullptr;
    // A failed view is not retried every frame; a resize or a new swapchain
    // tries again.
    bool back_buffer_failed_ = false;
};

} // namespace omvr::platform::d3d11
