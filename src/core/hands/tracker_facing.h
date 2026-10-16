// OpenMotherVR-Provenance: clean
#pragma once

#include "util/math/vec.h"

#include <optional>

// The motion tracker's facing from the left hand (spec 21 §6.2).
namespace omvr::core::hands {

// The horizontal facing (world x, z; unit length) the tracker scan should
// read for the hand's world aim direction `dir`: its horizontal part. When
// that is shorter than kTrackerMinHorizontal (the hand points nearly
// straight up or down) the previous facing is kept; before any, nullopt
// (the game's own facing stays). Never NaN.
class TrackerFacing final {
public:
    std::optional<util::Vec2> update(const util::Vec3& dir) noexcept;
    // The hand stopped driving the tracker: no previous facing to keep.
    void reset() noexcept { last_.reset(); }

private:
    std::optional<util::Vec2> last_;
};

} // namespace omvr::core::hands
