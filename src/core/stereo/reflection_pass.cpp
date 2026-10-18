// OpenMotherVR-Provenance: clean
// Planar reflections per eye (07). Analysis: dumps/07_reflections_head_pose.md.

#include "core/stereo/reflection_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace omvr::core::stereo {

namespace {

constexpr std::string_view kRenderedLog = "[stereo] reflections rendered per eye";
constexpr std::string_view kPovLog =
    "[stereo] reflection POV {} flags 0x{:02X} -> 0x{:02X} (state {}, size {}x{}, quality {})";
// 07 §6: the first viewport §4.8 had to move. If it never appears while
// per-eye renders run, the game keeps the origin (07 §8 item 1).
constexpr std::string_view kNarrowedLog =
    "[stereo] reflection pass {} viewport confined to its half of a "
    "{}-pixel-wide target: {},{} {}x{} -> {},{} {}x{}";

// The columns [lo, lo + width) of half `half` of a target `target_width`
// pixels wide (07 §4.8). 64-bit, so that x + width cannot overflow.
struct Columns {
    std::int64_t lo;
    std::int64_t width;
    std::int64_t hi() const noexcept { return lo + width; }
};

Columns half_columns(std::int32_t target_width, int half) noexcept {
    const std::int64_t width = std::max<std::int32_t>(target_width, 0) / 2;
    return {.lo = half * width, .width = width};
}

} // namespace

PixelViewport confine_viewport(const PixelViewport& vp, std::int32_t target_width, int half) noexcept {
    const Columns own        = half_columns(target_width, half);
    const Columns other      = half_columns(target_width, 1 - half);
    const std::int64_t left  = vp.x;
    const std::int64_t right = left + vp.width;
    const auto inside        = [&](const Columns& c) { return left >= c.lo && right <= c.hi(); };
    if (inside(own)) return vp;
    std::int64_t new_left  = 0;
    std::int64_t new_right = 0;
    if (left <= 0 && right >= target_width) { // the full width: the half
        new_left  = own.lo;
        new_right = own.hi();
    } else if (inside(other)) { // the origin was lost: the same place in this half
        new_left  = left + own.lo - other.lo;
        new_right = right + own.lo - other.lo;
    } else { // straddles the seam: what lies in this half
        new_left  = std::clamp(left, own.lo, own.hi());
        new_right = std::clamp(right, own.lo, own.hi());
    }
    PixelViewport out = vp;
    out.x             = static_cast<std::int32_t>(new_left); // within [0, target_width]
    out.width         = static_cast<std::int32_t>(new_right - new_left);
    return out;
}

ScreenMapping confine_screen_mapping(const ScreenMapping& m, std::int32_t target_width, int half) noexcept {
    if (target_width <= 0 || !std::isfinite(m.scale_x) || !std::isfinite(m.offset_x)) return m;
    // The constant comes from the integer rectangle divided by the width,
    // so rounding back gives that rectangle's columns.
    const auto width     = static_cast<float>(target_width);
    const auto to_pixels = [&](float fraction) {
        return static_cast<std::int32_t>(std::lround(fraction * width));
    };
    const PixelViewport vp = {.x = to_pixels(m.offset_x), .y = 0, .width = to_pixels(m.scale_x), .height = 0};
    const PixelViewport out = confine_viewport(vp, target_width, half);
    if (out == vp) return m;
    return {.scale_x  = static_cast<float>(out.width) / width,
            .scale_y  = m.scale_y,
            .offset_x = static_cast<float>(out.x) / width,
            .offset_y = m.offset_y};
}

void ReflectionPass::note_pov(const ReflectionPovInfo& info) noexcept {
    auto it =
        std::ranges::find_if(seen_, [&](const SeenPov& e) { return e.pov == info.pov || e.pov == nullptr; });
    if (it == seen_.end() || lines_ >= kReflectionLogLines) return;
    const bool pov_changed = it->pov == nullptr || it->flags != info.flags;
    if (!pov_changed && info.quality == quality_) return;
    ++lines_;
    diag::ports::logf(log_, kPovLog, info.pov, it->pov ? it->flags : -1, info.flags, info.state, info.width,
                      info.height, info.quality);
    *it      = {info.pov, info.flags};
    quality_ = info.quality;
}

bool ReflectionPass::take_capture_request() noexcept {
    // The capture is a DebugOptions diagnostic: with it off the key does
    // nothing.
    const bool down    = settings_.debug.debug && keyboard_.key_down(ports::Key::F10);
    const bool capture = down && !capture_key_down_;
    capture_key_down_  = down;
    return capture;
}

void ReflectionPass::note_rendered() noexcept {
    if (rendered_logged_.first()) log_.line(kRenderedLog);
}

void ReflectionPass::begin_pass(int half, std::uint32_t thread) noexcept {
    half_ = half;
    pass_thread_.store(thread, std::memory_order_relaxed);
}

void ReflectionPass::end_pass() noexcept { pass_thread_.store(0, std::memory_order_relaxed); }

PixelViewport ReflectionPass::pass_viewport(int half, const PixelViewport& vp,
                                            std::int32_t target_width) noexcept {
    const PixelViewport out = confine_viewport(vp, target_width, half);
    if (out != vp && narrowed_logged_.first()) {
        diag::ports::logf(log_, kNarrowedLog, half, target_width, vp.x, vp.y, vp.width, vp.height, out.x,
                          out.y, out.width, out.height);
    }
    return out;
}

} // namespace omvr::core::stereo
