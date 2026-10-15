// The desktop mirror's layout.
//
// From: FUN_1001eda0, the tail. Analysis: dumps/06a_stereo_interface.md.

#include "core/stereo/mirror_layout.h"

#include "core/config/settings_store.h"

#include <algorithm>
#include <cstring>

namespace omvr::core::stereo {

namespace {

using game::StereoInterface;

struct UvRect {
    float l, t, r, b;
};

void set_panel(MirrorConstants& c, int panel, const UvRect& draw, const UvRect& source) noexcept {
    const float d[4] = {draw.l, draw.t, draw.r, draw.b};
    const float s[4] = {source.l, source.t, source.r, source.b};
    std::memcpy(c.draw[panel], d, sizeof(d));
    std::memcpy(c.source[panel], s, sizeof(s));
}

// part / whole of a tangent, 1 when the whole is unknown.
float tan_ratio(float part, float whole) noexcept {
    return whole > 0.0f ? std::clamp(part / whole, 0.0f, 1.0f) : 1.0f;
}

// Eye e's real FOV (DefaultEyeFov) inside its half of the rendered image
// (MaxEyeFov, the FOV the eye was rendered with), in UV of the whole
// side-by-side texture: the compositor's crop bounds.
UvRect eye_crop(const StereoInterface& si, int e) noexcept {
    const auto& d = si.desc.default_eye_fov[e];
    const auto& x = si.desc.max_eye_fov[e];
    const float half = 0.5f * static_cast<float>(e);
    return {half + 0.5f * (0.5f - 0.5f * tan_ratio(d.left, x.left)),
            0.5f - 0.5f * tan_ratio(d.up, x.up),
            half + 0.5f * (0.5f + 0.5f * tan_ratio(d.right, x.right)),
            0.5f + 0.5f * tan_ratio(d.down, x.down)};
}

} // namespace

// Without "symmetric", each eye is cropped from the symmetric render FOV
// back to its real FOV; "both eyes" then shows eye 0's crop on the left and
// eye 1's on the right, over the union of their vertical ranges so the
// halves show the same rows and neither is stretched.
// From: FUN_1001eda0, the tail (which tests the type as 0 both, 2 left,
// otherwise right: the original's MirrorViewType integers). The original
// took the right eye's crop from the left half, drew "both eyes" cropped as
// eye 0 and its mirror image, and left the crop's height out of the draw
// height; it also showed whatever the game had bound at t0 in screen frames.
MirrorConstants mirror_constants(const StereoInterface& si, bool screen) noexcept {
    using config::MirrorViewType;
    const float tex_w = static_cast<float>(std::max(si.rt_width, 1));
    const float tex_h = static_cast<float>(std::max(si.rt_height, 1));
    const float window_aspect = si.mirror_width > 0 && si.mirror_height > 0
                                    ? static_cast<float>(si.mirror_width) / static_cast<float>(si.mirror_height)
                                    : tex_w / tex_h;
    // Pixel aspect of a source region du x dv.
    auto region_aspect = [&](float du, float dv) {
        return du > 0.0f && dv > 0.0f ? (du * tex_w) / (dv * tex_h) : window_aspect;
    };

    MirrorConstants c{};
    if (screen) {
        const UvRect source{0.0f, 0.25f, 0.5f, 0.75f};
        const float a = region_aspect(0.5f, 0.5f);
        if (a >= window_aspect) {   // bars above and below
            const float k = window_aspect / a;
            set_panel(c, 0, {0.0f, (1.0f - k) * 0.5f, 1.0f, (1.0f + k) * 0.5f}, source);
        } else {                    // bars at the sides
            const float k = a / window_aspect;
            set_panel(c, 0, {(1.0f - k) * 0.5f, 0.0f, (1.0f + k) * 0.5f, 1.0f}, source);
        }
        return c;
    }

    const auto& cfg = config::current().rendering;
    const MirrorViewType m = cfg.mirror_view_type;
    UvRect left_src{}, right_src{};
    bool both_cropped = false;
    if (cfg.symmetric_mirror_view) {
        if (m == MirrorViewType::LeftEye)       left_src = {0.0f, 0.0f, 0.5f, 1.0f};
        else if (m == MirrorViewType::BothEyes) left_src = {0.0f, 0.0f, 1.0f, 1.0f};
        else                                    left_src = {0.5f, 0.0f, 1.0f, 1.0f};
    } else if (m == MirrorViewType::BothEyes) {
        left_src  = eye_crop(si, 0);
        right_src = eye_crop(si, 1);
        const float v0 = std::min(left_src.t, right_src.t);
        const float v1 = std::max(left_src.b, right_src.b);
        left_src.t = right_src.t = v0;
        left_src.b = right_src.b = v1;
        both_cropped = true;
    } else {
        left_src = eye_crop(si, m == MirrorViewType::LeftEye ? 0 : 1);
    }

    const float du_left  = left_src.r - left_src.l;
    const float du_right = both_cropped ? right_src.r - right_src.l : 0.0f;
    const float k = window_aspect / region_aspect(du_left + du_right, left_src.b - left_src.t);
    const float top = (1.0f - k) * 0.5f, bottom = (1.0f + k) * 0.5f;
    if (both_cropped) {
        // Each half as wide as its crop's share of the summed width.
        const float split = du_left + du_right > 0.0f ? du_left / (du_left + du_right) : 0.5f;
        set_panel(c, 0, {0.0f, top, split, bottom}, left_src);
        set_panel(c, 1, {split, top, 1.0f, bottom}, right_src);
    } else {
        set_panel(c, 0, {0.0f, top, 1.0f, bottom}, left_src);
    }
    return c;
}

} // namespace omvr::core::stereo
