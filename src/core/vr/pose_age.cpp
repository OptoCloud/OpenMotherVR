#include "core/vr/pose_age.h"

#include "core/diag/tuning.h"

#include <cmath>

namespace omvr::core::vr {

std::optional<PoseAgeReport> PoseAge::add(std::int64_t frame_time, std::int64_t rendered_for,
                                          std::int64_t period) noexcept {
    // Unknown: no prediction recorded for the views, or no display period.
    const bool known = period > 0 && rendered_for != 0;
    const double periods =
        known ? static_cast<double>(frame_time - rendered_for) / static_cast<double>(period) : 0.0;
    const long long whole = std::llround(periods);
    if (known && whole == 0)
        ++same_;
    else if (known && whole == 1)
        ++one_early_;
    else
        ++other_;
    if (known) {
        sum_ += periods;
        ++known_;
    }
    if (++frames_ < diag::kDebugLogPeriodFrames) return std::nullopt;

    // The mean is over the frames whose age is known (11 §4.1).
    const PoseAgeReport report{frames_, same_, one_early_, other_, known_, known_ ? sum_ / known_ : 0.0};
    *this = PoseAge{};
    return report;
}

} // namespace omvr::core::vr
