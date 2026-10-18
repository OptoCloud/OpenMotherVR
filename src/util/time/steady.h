// OpenMotherVR-Provenance: clean
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

// Steady-clock time as the mod's logic uses it: one time point type, float
// seconds for tuning values, and a time point handed between threads as one
// 64-bit word.
namespace omvr::util {

using SteadyClock = std::chrono::steady_clock;
using TimePoint   = SteadyClock::time_point;
using Seconds     = std::chrono::duration<float>;

// `t` plus `d`, `d` truncated to the clock's resolution.
[[nodiscard]] constexpr TimePoint after(TimePoint t, Seconds d) noexcept {
    return t + std::chrono::duration_cast<SteadyClock::duration>(d);
}

// From `from` to `to` in float seconds (negative when `to` is earlier).
[[nodiscard]] constexpr Seconds seconds_between(TimePoint from, TimePoint to) noexcept {
    return std::chrono::duration_cast<Seconds>(to - from);
}

// A time point as clock ticks with 0 meaning "none": at least 1, so a time
// at the clock's epoch is still a time. For a trivially copyable value
// handed through util::SeqLock.
[[nodiscard]] constexpr std::int64_t stamp_ticks(TimePoint t) noexcept {
    return std::max<std::int64_t>(t.time_since_epoch().count(), 1);
}
[[nodiscard]] constexpr std::optional<TimePoint> from_stamp_ticks(std::int64_t ticks) noexcept {
    if (ticks == 0) return std::nullopt;
    return TimePoint{SteadyClock::duration{ticks}};
}

// A time point written on one thread and read on others, or none. Release
// and acquire: a reader that sees the time also sees what was stored before
// it.
class AtomicStamp {
public:
    void store(TimePoint t) noexcept { ticks_.store(stamp_ticks(t), std::memory_order_release); }
    [[nodiscard]] std::optional<TimePoint> load() const noexcept {
        return from_stamp_ticks(ticks_.load(std::memory_order_acquire));
    }

private:
    std::atomic<std::int64_t> ticks_{0};
};

} // namespace omvr::util
