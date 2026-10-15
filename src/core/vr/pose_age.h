// OpenMotherVR-Provenance: clean
#pragma once

#include <cstdint>
#include <optional>

namespace omvr::core::vr {

// The pose-age counts over one reporting period (11 §4.1).
struct PoseAgeReport {
    std::uint32_t frames    = 0;   // frames counted
    std::uint32_t same      = 0;   // views predicted for the frame's own display time
    std::uint32_t one_early = 0;   // one display period early
    std::uint32_t other     = 0;   // anything else, or unknown
    std::uint32_t known     = 0;   // frames whose age is known
    double mean             = 0.0; // mean age in periods, over the known frames
};

// How many display periods before a frame's own display time its declared
// (and rendered) views were predicted for. 0 is right; a loop that renders
// with the previous frame's views measures 1. Render thread.
class PoseAge {
public:
    // One submitted frame displayed at `frame_time`, whose views were
    // predicted for `rendered_for`, with display period `period` (runtime
    // time units; 0 means not known). Every kDebugLogPeriodFrames frames
    // returns the counts and starts over.
    std::optional<PoseAgeReport> add(std::int64_t frame_time, std::int64_t rendered_for,
                                     std::int64_t period) noexcept;

private:
    std::uint32_t frames_ = 0, same_ = 0, one_early_ = 0, other_ = 0, known_ = 0;
    double sum_ = 0.0;
};

} // namespace omvr::core::vr
