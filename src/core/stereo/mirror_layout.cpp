// The desktop mirror's layout (03 §8).
//
// From: FUN_1001eda0, the tail. Analysis: dumps/06a_stereo_interface.md.

#include "core/stereo/mirror_layout.h"

#include "core/stereo/tuning.h"

#include <algorithm>
#include <iterator>

namespace omvr::core::stereo {

namespace {

using config::MirrorViewType;

// A region in UV: left, top, right, bottom (v down).
struct UvRect {
    float l, t, r, b;
};

// The whole of each half of the side-by-side texture, and all of it.
constexpr UvRect kLeftHalf{0.0f, 0.0f, 0.5f, 1.0f};
constexpr UvRect kRightHalf{0.5f, 0.0f, 1.0f, 1.0f};
constexpr UvRect kWhole{0.0f, 0.0f, 1.0f, 1.0f};
constexpr UvRect kScreenSource{kScreenMirrorSource[0], kScreenMirrorSource[1], kScreenMirrorSource[2],
                               kScreenMirrorSource[3]};

void set_panel(MirrorConstants& c, int panel, const UvRect& draw, const UvRect& source) noexcept {
    const float d[4] = {draw.l, draw.t, draw.r, draw.b};
    const float s[4] = {source.l, source.t, source.r, source.b};
    std::copy(std::begin(d), std::end(d), c.draw[panel]);
    std::copy(std::begin(s), std::end(s), c.source[panel]);
}

// part / whole of a tangent, 1 when the whole is unknown.
float tan_ratio(float part, float whole) noexcept {
    return whole > 0.0f ? std::clamp(part / whole, 0.0f, 1.0f) : 1.0f;
}

// Eye e's real FOV inside its half of the rendered image, in UV of the whole
// side-by-side texture: the compositor's crop bounds (03 §3.3).
UvRect eye_crop(const MirrorView& view, int e) noexcept {
    const auto& d    = view.real_fov[e];
    const auto& x    = view.rendered_fov[e];
    const float half = 0.5f * static_cast<float>(e);
    return {half + 0.5f * (0.5f - 0.5f * tan_ratio(d.left, x.left)), 0.5f - 0.5f * tan_ratio(d.up, x.up),
            half + 0.5f * (0.5f + 0.5f * tan_ratio(d.right, x.right)),
            0.5f + 0.5f * tan_ratio(d.down, x.down)};
}

// Aspects in pixels: the window's, and a source region's.
struct Aspects {
    float tex_w;
    float tex_h;
    float window;

    // Pixel aspect of a source region du x dv, the window's when empty.
    float region(float du, float dv) const noexcept {
        return du > 0.0f && dv > 0.0f ? (du * tex_w) / (dv * tex_h) : window;
    }
};

Aspects aspects_of(const MirrorView& view) noexcept {
    const float tex_w  = static_cast<float>(std::max(view.texture.width, 1));
    const float tex_h  = static_cast<float>(std::max(view.texture.height, 1));
    const float window = view.window.width > 0 && view.window.height > 0
                             ? static_cast<float>(view.window.width) / static_cast<float>(view.window.height)
                             : tex_w / tex_h;
    return {tex_w, tex_h, window};
}

// Screen frames: the virtual screen's region fitted whole, with bars above
// and below or at the sides.
MirrorConstants screen_constants(const Aspects& a) noexcept {
    MirrorConstants c{};
    const float region = a.region(kScreenSource.r - kScreenSource.l, kScreenSource.b - kScreenSource.t);
    if (region >= a.window) { // bars above and below
        const float k = a.window / region;
        set_panel(c, 0, {0.0f, (1.0f - k) * 0.5f, 1.0f, (1.0f + k) * 0.5f}, kScreenSource);
    } else { // bars at the sides
        const float k = region / a.window;
        set_panel(c, 0, {(1.0f - k) * 0.5f, 0.0f, (1.0f + k) * 0.5f, 1.0f}, kScreenSource);
    }
    return c;
}

// The source region(s) of head-tracked frames. Without "symmetric", each
// eye is cropped from the symmetric render FOV back to its real FOV; "both
// eyes" then shows eye 0's crop on the left and eye 1's on the right, over
// the union of their vertical ranges so the halves show the same rows and
// neither is stretched.
struct EyeSources {
    UvRect left{};
    UvRect right{};
    bool both_cropped = false;
};

EyeSources eye_sources(const MirrorView& view) noexcept {
    const MirrorViewType m = view.type;
    if (view.symmetric) {
        if (m == MirrorViewType::LeftEye) return {kLeftHalf};
        if (m == MirrorViewType::BothEyes) return {kWhole};
        return {kRightHalf};
    }
    if (m != MirrorViewType::BothEyes) return {eye_crop(view, m == MirrorViewType::LeftEye ? 0 : 1)};
    EyeSources s{eye_crop(view, 0), eye_crop(view, 1), true};
    const float v0 = std::min(s.left.t, s.right.t);
    const float v1 = std::max(s.left.b, s.right.b);
    s.left.t = s.right.t = v0;
    s.left.b = s.right.b = v1;
    return s;
}

} // namespace

// From: FUN_1001eda0, the tail (which tests the type as 0 both, 2 left,
// otherwise right: the original's MirrorViewType integers). The original
// took the right eye's crop from the left half, drew "both eyes" cropped as
// eye 0 and its mirror image, and left the crop's height out of the draw
// height; it also showed whatever the game had bound at t0 in screen frames.
MirrorConstants mirror_constants(const MirrorView& view, bool screen) noexcept {
    const Aspects a = aspects_of(view);
    if (screen) return screen_constants(a);

    const EyeSources src = eye_sources(view);
    MirrorConstants c{};
    const float du_left  = src.left.r - src.left.l;
    const float du_right = src.both_cropped ? src.right.r - src.right.l : 0.0f;
    const float k        = a.window / a.region(du_left + du_right, src.left.b - src.left.t);
    const float top = (1.0f - k) * 0.5f, bottom = (1.0f + k) * 0.5f;
    if (src.both_cropped) {
        // Each half as wide as its crop's share of the summed width.
        const float split = du_left + du_right > 0.0f ? du_left / (du_left + du_right) : 0.5f;
        set_panel(c, 0, {0.0f, top, split, bottom}, src.left);
        set_panel(c, 1, {split, top, 1.0f, bottom}, src.right);
    } else {
        set_panel(c, 0, {0.0f, top, 1.0f, bottom}, src.left);
    }
    return c;
}

} // namespace omvr::core::stereo
