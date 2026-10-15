// From: FUN_100202a0 (the GetRenderTargetSize hook) and the device slot 16.
// Analysis: dumps/06a_stereo_interface.md.

#include "core/stereo/render_target.h"

#include "core/config/settings_store.h"
#include "core/diag/logger.h"
#include "game/stereo/stereo_objects.h" // R: stereo (A3): the state as a value

#include <algorithm>

namespace omvr::core::stereo {

using game::StereoInterface;
using game::stereo::load_state;

void set_target_size(StereoInterface& si, int width, int height) noexcept {
    si.rt_width    = width;
    si.rt_height   = height;
    si.viewport[0] = {0, 0, width / 2, height};
    si.viewport[1] = {width / 2, 0, width / 2, height};
}

void render_target_size(StereoInterface& si, vr::ports::VrRuntime& backend, int* w, int* h) {
    // PixelScale multiplies the runtime's recommended size, which already
    // includes SteamVR's own resolution setting. Capped so the side-by-side
    // target stays within D3D11's 16384 texture limit.
    constexpr float kMaxTextureSize = 16384.0f;
    const vr::TextureSize l1 = backend.eye_texture_size(0, 1.0f);
    const vr::TextureSize r1 = backend.eye_texture_size(1, 1.0f);
    float scale = config::current().rendering.pixel_scale;
    if (l1.w + r1.w > 0 && l1.h > 0 && r1.h > 0) {
        const float max_scale = std::min(kMaxTextureSize / static_cast<float>(l1.w + r1.w),
                                         kMaxTextureSize / static_cast<float>(std::max(l1.h, r1.h)));
        scale = std::min(scale, max_scale);
    }
    const vr::TextureSize l = backend.eye_texture_size(0, scale);
    const vr::TextureSize r = backend.eye_texture_size(1, scale);
    const int width  = l.w + r.w;
    // The taller eye, so neither eye's image is cut off. The original took
    // the height of the wider eye.
    const int height = std::max(l.h, r.h);

    // Keep the eye layout of the texture in use until the game makes one of
    // the new size (it then calls ConfigureRendering again).
    const bool configured = si.configured_width != 0;
    if (!configured || (width == si.configured_width && height == si.configured_height)) {
        set_target_size(si, width, height);
    }
    static int s_last_w = 0, s_last_h = 0;
    if (width != s_last_w || height != s_last_h) {
        s_last_w = width;
        s_last_h = height;
        diag::logf("[stereo] render target size {}x{} ({:.1f} MP, pixel scale {:.2f}; state {}, texture {}x{})",
                   width, height, static_cast<double>(width) * height / 1e6, scale,
                   static_cast<int>(load_state(si)), si.configured_width, si.configured_height);
    }
    *w = width;
    *h = height;
}

} // namespace omvr::core::stereo
