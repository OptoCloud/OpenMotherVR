// OpenMotherVR-Provenance: clean
#pragma once

#include "core/stereo/types.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

// The comfort vignette and the snap-turn fade (spec 22 §4.3, §5): what the
// game thread decides, handed to the render thread, and the darkening that
// slot 6 draws into each eye before the frame goes to the runtime.
namespace omvr::core::stereo {

using ComfortTime = std::chrono::steady_clock::time_point;

// The vignette's radii as angles from the eye's forward axis, in radians.
struct VignetteRadii {
    float inner_rad = 0.0f;
    float outer_rad = 0.0f;
};

// The radii for shown strength `strength` in (0, 1]: inner 60 - 35 * k
// degrees, outer 15 degrees further. At 0 or below, radii no pixel reaches
// (beyond 90 degrees), so nothing darkens.
VignetteRadii vignette_radii(float strength) noexcept;

// The angle from the eye's forward axis of the pixel at (u, v) in its eye
// half (0..1, v down) for the half-FOV tangents `tan_h`, `tan_v`.
float vignette_angle(float u, float v, float tan_h, float tan_v) noexcept;

// The darkening factor at `angle_rad`: 1 inside the inner radius, 0 beyond
// the outer, smooth in between. vignette.hlsl computes the same.
float vignette_factor(float angle_rad, const VignetteRadii& radii) noexcept;

// The shown strength following its target: 0 to full in kVignetteRiseTime,
// full to 0 in kVignetteFallTime, whatever the setting's value. Falling, it
// keeps the last strength the target had. Render thread.
class VignetteRamp final {
public:
    float advance(float target, std::chrono::steady_clock::duration dt) noexcept;
    void reset() noexcept { share_ = 0.0f; }

private:
    float share_ = 0.0f; // 0..1 of `full_`
    float full_  = 0.0f;
};

// The snap-turn fade: black on the frame that sees a new step count, back
// to clear over kSnapFadeTime; a further step restarts it. Render thread.
class SnapFade final {
public:
    // The darkness for this frame, 0 clear to 1 black.
    float darkness(std::uint32_t steps, ComfortTime now) noexcept;

private:
    std::uint32_t seen_ = 0;
    std::optional<ComfortTime> start_;
};

// The hand-over between the threads and the render thread's per-frame
// decision. The game thread publishes the vignette's target strength and
// counts snap steps that want a fade (spec 22 §15: atomic 32-bit values);
// slot 6 turns them into the pass's constants.
class ComfortView final {
public:
    // Game thread: the target strength for the current game frame
    // (`Comfort.Vignette` while moving or turning, else 0).
    void publish_target(float strength) noexcept { target_.store(strength, std::memory_order_relaxed); }
    // Game thread: a snap step was made with the fade on.
    void note_snap_step() noexcept { steps_.fetch_add(1, std::memory_order_relaxed); }

    // Slot 6 of each frame, render thread: the constants for the darkening
    // pass, or nullopt when nothing is to be drawn (vignette at 0, no fade
    // running, or a screen frame). `tan_h` and `tan_v` are the eye's half-FOV
    // tangents. No allocation.
    std::optional<VignetteConstants> frame(ComfortTime now, bool head_tracked, float tan_h,
                                           float tan_v) noexcept;

private:
    // Relaxed: each value stands alone, no other data hangs on it.
    std::atomic<float> target_{0.0f};
    std::atomic<std::uint32_t> steps_{0};
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

    // Render thread only.
    VignetteRamp ramp_;
    SnapFade fade_;
    std::optional<ComfortTime> last_frame_;
};

} // namespace omvr::core::stereo
