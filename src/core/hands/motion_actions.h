// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/tuning.h"
#include "core/hands/types.h"
#include "util/math/vec.h"

#include <array>
#include <cstddef>
#include <optional>

// Melee and throwing by motion (spec 21 §8, §9): a real swing of the right
// hand presses the game's fire action once, and releasing the Aim control
// with the hand moving fast enough throws the item in hand through the
// same press; the right arm is handed to the game's melee animation while
// the player's melee action runs (§8.2, r11). Game thread.
namespace omvr::core::hands {

// 21 §8.2: a swing is the right hand's speed rising above kSwingStartMps
// and then falling below kSwingEndMps within kSwingWindow.
class SwingDetector final {
public:
    // One update with the hand's speed in metres per second (nullopt: not
    // known, which drops a swing in progress). True on the update that ends
    // a swing.
    bool update(std::optional<float> speed_mps, TimePoint now) noexcept;
    void reset() noexcept;

private:
    std::optional<TimePoint> fast_since_;
    // A fast movement that did not end in time: no new swing until the hand
    // has slowed down.
    bool wait_for_rest_ = false;
};

// 21 §8.2: each swing presses fire for kMotionPress, except within
// kSwingLockout of the last press.
class MeleeByMotion final {
public:
    // `allowed`: Hands.MeleeByMotion, tracked hands active, the right hand
    // tracked and the melee tool or the stun baton current. Returns the end
    // of the fire press this update starts, if it starts one.
    std::optional<TimePoint> update(bool allowed, std::optional<float> speed_mps, TimePoint now) noexcept;

    // A press by motion began within kMotionSwingAttribution before `t`:
    // a melee start at `t` is that swing (for the log, 21 §18).
    bool pressed_shortly_before(TimePoint t) const noexcept;

private:
    SwingDetector swing_;
    std::optional<TimePoint> last_press_;
};

// 21 §8.2 (r11): from each start of the player's melee action the right
// arm plays the game's animation: the tracked right hand's weight falls to
// 0 over kMeleeBlendOut, stays 0 until kMeleeHold after the start, then
// returns to 1 over kMeleeBlendBack. A start during that time restarts the
// hold without a jump.
class ArmHandOff final {
public:
    void start(TimePoint now) noexcept;
    // The right hand's weight factor: 1 tracked, 0 the game's animation.
    float weight(TimePoint now) const noexcept;
    // A hand-off began and has not ended by `now`.
    bool running(TimePoint now) const noexcept;

private:
    std::optional<TimePoint> out_from_; // the blend out started here
    TimePoint hold_until_{};
};

// A release of the Aim control while a throw was armed (21 §9.2).
struct ThrowRelease {
    bool thrown     = false; // fast enough: the fire press throws the item
    float speed_mps = 0.0f;  // the peak speed over the window before the release
    util::Vec3 local{};      // that sample's velocity relative to the head (camera axes, game units/s)
    TimePoint at{};          // when the control was let go
};

// 21 §9.2: holding the Aim control with a throwable arms a throw; letting
// it go throws when the hand's peak speed over the last
// kThrowVelocityWindow is at least kThrowReleaseMps, and cancels otherwise.
class ThrowByMotion final {
public:
    // `allowed`: Hands.ThrowByMotion, tracked hands active, the right hand
    // tracked and no weapon object current (a throwable, 21 §5.2).
    // `aim_held`: the Aim control as the game is given it. `local`: the
    // grip's velocity relative to the head (camera axes, game units per
    // second), nullopt when not known; `world_scale` turns it into metres.
    std::optional<ThrowRelease> update(bool allowed, bool aim_held, const std::optional<util::Vec3>& local,
                                       float world_scale, TimePoint now) noexcept;

private:
    struct Sample {
        TimePoint at{};
        util::Vec3 local{};
        float speed_mps = 0.0f;
    };
    void disarm() noexcept;
    ThrowRelease release(TimePoint now) const noexcept;

    std::array<Sample, kThrowSamples> samples_{};
    std::size_t next_  = 0;
    std::size_t count_ = 0;
    bool armed_        = false;
};

// The speed in metres per second of a velocity in game units per second.
float speed_mps(const util::Vec3& velocity, float world_scale) noexcept;

// 21 §9.2: the thrown item's launch velocity in the world for a release
// velocity `local` relative to the head: rotated by the active camera
// (3.1's rotation only), its horizontal part times Hands.ThrowStrength
// (`strength`), its vertical part unchanged (r11).
util::Vec3 throw_velocity(const CameraPose& camera, const util::Vec3& local, float strength) noexcept;

} // namespace omvr::core::hands
