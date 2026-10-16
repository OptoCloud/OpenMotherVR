// OpenMotherVR-Provenance: clean
#include "core/hands/hand_mapping.h"

#include "util/math/quat.h"

namespace omvr::core::hands {

HeadLocalPose head_local(const util::Matrix34& head, const util::Matrix34& hand) noexcept {
    const util::Vec3 d = util::sub(util::translation_of(hand), util::translation_of(head));
    return {util::rotate_inverse(head, d), util::quat_from_rotation(util::relative_rotation(head, hand))};
}

WorldPose to_world(const CameraPose& camera, const HeadLocalPose& local) noexcept {
    return {util::add(camera.position, util::rotate(camera.orientation, local.offset)),
            util::normalized(util::multiply(camera.orientation, local.rotation))};
}

util::Vec3 pointing(const WorldPose& pose) noexcept {
    return util::rotate(pose.orientation, {0.0f, 0.0f, 1.0f});
}

} // namespace omvr::core::hands
