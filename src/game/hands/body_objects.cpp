// OpenMotherVR-Provenance: clean
#include "game/hands/body_objects.h"

#include "game/code_address.h"

#include <cstring>

namespace omvr::game::hands {

namespace {

const std::uint8_t* at(const void* object, std::ptrdiff_t offset) noexcept {
    return static_cast<const std::uint8_t*>(object) + offset;
}

template <class T>
T read(const void* object, std::ptrdiff_t offset) noexcept {
    T v;
    std::memcpy(&v, at(object, offset), sizeof(T));
    return v;
}

void* pointer_at(const void* object, std::ptrdiff_t offset) noexcept {
    return CodeAddress{read<std::uint32_t>(object, offset)};
}

// A Havok string pointer's characters, up to kMaxBoneName of them.
std::string_view havok_string(const void* object, std::ptrdiff_t offset) noexcept {
    const std::uint32_t raw =
        read<std::uint32_t>(object, offset) & ~static_cast<std::uint32_t>(kStringPointerFlags);
    const char* s = CodeAddress{raw};
    if (!s) return {};
    std::size_t n = 0;
    while (n < kMaxBoneName && s[n] != '\0') ++n;
    return {s, n};
}

} // namespace

std::optional<PlayerPose> player_pose(void* character) noexcept {
    if (!character) return std::nullopt;
    void* animation  = static_cast<std::uint8_t*>(character) + kCharacterAnimation;
    const void* pose = pointer_at(animation, kAnimationPose);
    if (!pose) return std::nullopt;
    const void* skeleton = pointer_at(pose, kPoseSkeleton);
    if (!skeleton) return std::nullopt;
    return PlayerPose{animation, pose, skeleton};
}

std::string_view skeleton_name(const void* skeleton) noexcept {
    return havok_string(skeleton, kSkeletonName);
}

bool read_skeleton(const void* skeleton, SkeletonRead& out) noexcept {
    out.count           = 0;
    const auto n        = read<std::int32_t>(skeleton, kSkeletonBoneCount);
    const void* bones   = pointer_at(skeleton, kSkeletonBones);
    const void* parents = pointer_at(skeleton, kSkeletonParents);
    if (n <= 0 || !bones || !parents) return false;
    out.count = static_cast<std::size_t>(n);
    if (out.count > kMaxSkeletonBones) return false;
    for (std::size_t i = 0; i < out.count; ++i) {
        const auto offset = static_cast<std::ptrdiff_t>(i);
        out.names[i]      = havok_string(bones, offset * kSkeletonBoneStride);
        out.parents[i] =
            read<std::int16_t>(parents, offset * static_cast<std::ptrdiff_t>(sizeof(std::int16_t)));
        if (out.names[i].empty()) return false;
    }
    return true;
}

const void* pose_model_data(const void* pose) noexcept { return pointer_at(pose, kPoseModelData); }

const void* pose_skeleton(const void* pose) noexcept { return pointer_at(pose, kPoseSkeleton); }

std::int32_t pose_model_count(const void* pose) noexcept { return read<std::int32_t>(pose, kPoseModelCount); }

bool is_model_object(const void* animation, const void* object) noexcept {
    if (pointer_at(animation, kAnimationModelObject) == object) return true;
    for (std::size_t i = 0; i < kAnimationPartCount; ++i) {
        const auto offset = kAnimationPartObjects + static_cast<std::ptrdiff_t>(i) * kAnimationPartStride;
        if (pointer_at(animation, offset) == object) return true;
    }
    return false;
}

std::optional<bool> requested_crouch(const void* character) noexcept {
    if (!character) return std::nullopt;
    return read<std::uint8_t>(character, kCharacterRequestedCrouch) != 0;
}

} // namespace omvr::game::hands
