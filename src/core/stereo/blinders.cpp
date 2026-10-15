#include "core/stereo/blinders.h"

#include <bit>

namespace omvr::core::stereo {

namespace {

// 1e-5, the game's own constant, bit for bit.
constexpr float kHeadClipOn  = std::bit_cast<float>(0x3727C5ACu);
constexpr float kHeadClipOff = 1.0f;
float g_head_clip_threshold = kHeadClipOn;

} // namespace

float& head_clip_threshold() noexcept { return g_head_clip_threshold; }

void set_clipping_blinders(bool on) noexcept {
    g_head_clip_threshold = on ? kHeadClipOn : kHeadClipOff;
}

} // namespace omvr::core::stereo
