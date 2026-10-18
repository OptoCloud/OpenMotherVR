// OpenMotherVR-Provenance: clean
#include "core/hands/motion_actions.h"

#include "util/math/quat.h"

#include <algorithm>
#include <cmath>

namespace omvr::core::hands {

namespace {

using util::after;
using util::seconds_between;

// A blend's progress from `from` over `length`: 0 at `from`, 1 at the end.
float progress(TimePoint from, TimePoint now, Seconds length) noexcept {
    return std::clamp(seconds_between(from, now).count() / length.count(), 0.0f, 1.0f);
}

} // namespace

// ---- Swing (21 §8.2) -----------------------------------------------------------

bool SwingDetector::update(std::optional<float> speed_mps, TimePoint now) noexcept {
    if (!speed_mps) {
        reset();
        return false;
    }
    if (wait_for_rest_) {
        wait_for_rest_ = !(*speed_mps < kSwingEndMps);
        return false;
    }
    if (!fast_since_) {
        if (*speed_mps > kSwingStartMps) fast_since_ = now;
        return false;
    }
    if (*speed_mps < kSwingEndMps && now <= after(*fast_since_, kSwingWindow)) {
        fast_since_.reset();
        return true;
    }
    if (now > after(*fast_since_, kSwingWindow)) {
        // Too long fast for a swing (a run, a turn of the body).
        fast_since_.reset();
        wait_for_rest_ = !(*speed_mps < kSwingEndMps);
    }
    return false;
}

void SwingDetector::reset() noexcept {
    fast_since_.reset();
    wait_for_rest_ = false;
}

std::optional<TimePoint> MeleeByMotion::update(bool allowed, std::optional<float> speed_mps,
                                               TimePoint now) noexcept {
    if (!allowed) {
        swing_.reset();
        return std::nullopt;
    }
    if (!swing_.update(speed_mps, now)) return std::nullopt;
    if (last_press_ && now < after(*last_press_, kSwingLockout)) return std::nullopt;
    last_press_ = now;
    return after(now, kMotionPress);
}

bool MeleeByMotion::pressed_shortly_before(TimePoint t) const noexcept {
    return last_press_ && *last_press_ <= t && t <= after(*last_press_, kMotionSwingAttribution);
}

// ---- Arm hand-off (21 §8.2, r11) -----------------------------------------------

void ArmHandOff::start(TimePoint now) noexcept {
    // Restarted on the way back: the blend out continues from the weight
    // reached, so the arm does not jump.
    const float w = weight(now);
    out_from_     = now - std::chrono::duration_cast<TimePoint::duration>(kMeleeBlendOut * (1.0f - w));
    hold_until_   = after(now, kMeleeHold);
}

float ArmHandOff::weight(TimePoint now) const noexcept {
    if (!out_from_) return 1.0f;
    const float out  = 1.0f - progress(*out_from_, now, kMeleeBlendOut);
    const float back = now <= hold_until_ ? 0.0f : progress(hold_until_, now, kMeleeBlendBack);
    return std::max(out, back);
}

bool ArmHandOff::running(TimePoint now) const noexcept {
    return out_from_ && now < after(hold_until_, kMeleeBlendBack);
}

// ---- Throw (21 §9.2) -----------------------------------------------------------

float speed_mps(const util::Vec3& velocity, float world_scale) noexcept {
    const float length = util::length(velocity);
    return world_scale > 0.0f ? length / world_scale : length;
}

void ThrowByMotion::disarm() noexcept {
    armed_ = false;
    count_ = 0;
    next_  = 0;
}

ThrowRelease ThrowByMotion::release(TimePoint now) const noexcept {
    ThrowRelease r;
    r.at = now;
    for (std::size_t i = 0; i < count_; ++i) {
        const Sample& s = samples_[i];
        if (s.at + std::chrono::duration_cast<TimePoint::duration>(kThrowVelocityWindow) < now) continue;
        if (s.speed_mps > r.speed_mps) {
            r.speed_mps = s.speed_mps;
            r.local     = s.local;
        }
    }
    r.thrown = r.speed_mps >= kThrowReleaseMps;
    return r;
}

std::optional<ThrowRelease> ThrowByMotion::update(bool allowed, bool aim_held,
                                                  const std::optional<util::Vec3>& local, float world_scale,
                                                  TimePoint now) noexcept {
    if (!allowed) {
        disarm();
        return std::nullopt;
    }
    if (!aim_held && !armed_) return std::nullopt;
    // The release's own sample counts too: the hand is still moving as the
    // grip opens.
    if (local) {
        samples_[next_] = {now, *local, speed_mps(*local, world_scale)};
        next_           = (next_ + 1) % samples_.size();
        count_          = std::min(count_ + 1, samples_.size());
    }
    if (aim_held) {
        armed_ = true;
        return std::nullopt;
    }
    const ThrowRelease r = release(now);
    disarm();
    return r;
}

util::Vec3 throw_velocity(const CameraPose& camera, const util::Vec3& local, float strength) noexcept {
    const util::Vec3 world = util::rotate(camera.orientation, local);
    return {world.x * strength, world.y, world.z * strength};
}

} // namespace omvr::core::hands
