// OpenMotherVR-Provenance: clean
#include "core/hands/tracker_facing.h"

#include "core/hands/tuning.h"

#include <cmath>

namespace omvr::core::hands {

std::optional<util::Vec2> TrackerFacing::update(const util::Vec3& dir) noexcept {
    const float horizontal = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    if (horizontal >= kTrackerMinHorizontal) last_ = util::Vec2{dir.x / horizontal, dir.z / horizontal};
    return last_;
}

} // namespace omvr::core::hands
