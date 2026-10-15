#include "core/stereo/blinders.h"

#include "game/constants.h"

#include <bit>
#include <cstdint>

namespace omvr::core::stereo {

namespace {

// 1e-5, the game's own constant, bit for bit (05 §7.2).
constexpr float kHeadClipOn = std::bit_cast<float>(game::kHeadClipThresholdBits);
// A visibility never below 1: the view is never blacked out.
constexpr float kHeadClipOff = 1.0f;

// The game reads it through the patched comparison, so its address must
// stay fixed: namespace-scope storage, trivially destructible.
float threshold = kHeadClipOn;

} // namespace

float& head_clip_threshold() noexcept { return threshold; }

void set_clipping_blinders(bool on) noexcept { threshold = on ? kHeadClipOn : kHeadClipOff; }

} // namespace omvr::core::stereo
