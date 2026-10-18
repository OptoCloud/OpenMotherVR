// OpenMotherVR-Provenance: clean
#include "game/hands/body_objects.h"

#include "game/code_address.h"
#include "game/field.h"
#include "game/hands/skeleton_names.h"

#include <algorithm>
#include <span>

namespace omvr::game::hands {

namespace {

static_assert(kFirstPersonBoneCount <= kMaxSkeletonBones);

// A Havok string pointer's characters, up to kMaxBoneName of them.
std::string_view havok_string(const void* object, std::ptrdiff_t offset) noexcept {
    const std::uint32_t raw =
        read_field<std::uint32_t>(object, offset) & ~static_cast<std::uint32_t>(kStringPointerFlags);
    const char* s = CodeAddress{raw};
    if (!s) return {};
    std::size_t n = 0;
    while (n < kMaxBoneName && s[n] != '\0') ++n;
    return {s, n};
}

// The parent index of bone `i`.
std::int16_t parent_at(const void* parents, std::size_t i) noexcept {
    return read_field<std::int16_t>(
        parents, static_cast<std::ptrdiff_t>(i) * static_cast<std::ptrdiff_t>(sizeof(std::int16_t)));
}

// 21 §2.3 (r10): a first-person skeleton carries no bone names; it is read
// with the table's when its count and parents show it is the table's
// skeleton. Only the parents are read: its bone array has no names.
SkeletonReadInfo read_table_skeleton(const void* skeleton, SkeletonReadInfo info,
                                     SkeletonRead& out) noexcept {
    const void* parents = pointer_field(skeleton, kSkeletonParents);
    if (!parents) {
        info.result = SkeletonReadResult::NoArrays;
        return info;
    }
    const auto count   = static_cast<std::size_t>(info.raw_count);
    const auto checked = std::min(count, kFirstPersonBoneCount);
    for (std::size_t i = 0; i < checked; ++i) out.parents[i] = parent_at(parents, i);
    const std::size_t differs = first_table_difference(count, std::span{out.parents}.first(checked));
    if (differs != kNoDifference) {
        info.result = SkeletonReadResult::TableMismatch;
        info.bone   = differs;
        return info;
    }
    std::ranges::copy(kFirstPersonBoneNames, out.names.begin());
    out.count            = kFirstPersonBoneCount;
    out.names_from_table = true;
    return info;
}

} // namespace

std::optional<PlayerPose> player_pose(void* character) noexcept {
    if (!character) return std::nullopt;
    void* animation  = static_cast<std::uint8_t*>(character) + kCharacterAnimation;
    const void* pose = pointer_field(animation, kAnimationPose);
    if (!pose) return std::nullopt;
    const void* skeleton = pointer_field(pose, kPoseSkeleton);
    if (!skeleton) return std::nullopt;
    return PlayerPose{animation, pose, skeleton};
}

std::string_view skeleton_name(const void* skeleton) noexcept {
    return havok_string(skeleton, kSkeletonName);
}

SkeletonReadInfo read_skeleton(const void* skeleton, SkeletonRead& out) noexcept {
    out.count            = 0;
    out.names_from_table = false;
    SkeletonReadInfo info;
    info.raw_count = read_field<std::int32_t>(skeleton, kSkeletonBoneCount);
    if (info.raw_count <= 0) {
        info.result = SkeletonReadResult::NoBones;
        return info;
    }
    if (uses_first_person_table(skeleton_name(skeleton))) return read_table_skeleton(skeleton, info, out);
    const void* bones   = pointer_field(skeleton, kSkeletonBones);
    const void* parents = pointer_field(skeleton, kSkeletonParents);
    if (!bones || !parents) {
        info.result = SkeletonReadResult::NoArrays;
        return info;
    }
    out.count = static_cast<std::size_t>(info.raw_count);
    if (out.count > kMaxSkeletonBones) {
        info.result = SkeletonReadResult::TooManyBones;
        return info;
    }
    for (std::size_t i = 0; i < out.count; ++i) {
        out.names[i]   = havok_string(bones, static_cast<std::ptrdiff_t>(i) * kSkeletonBoneStride);
        out.parents[i] = parent_at(parents, i);
        if (out.names[i].empty()) {
            info.result = SkeletonReadResult::UnnamedBone;
            info.bone   = i;
            return info;
        }
    }
    return info;
}

const void* pose_model_data(const void* pose) noexcept { return pointer_field(pose, kPoseModelData); }

const void* pose_skeleton(const void* pose) noexcept { return pointer_field(pose, kPoseSkeleton); }

std::int32_t pose_model_count(const void* pose) noexcept {
    return read_field<std::int32_t>(pose, kPoseModelCount);
}

bool is_model_object(const void* animation, const void* object) noexcept {
    if (pointer_field(animation, kAnimationModelObject) == object) return true;
    for (std::size_t i = 0; i < kAnimationPartCount; ++i) {
        const auto offset = kAnimationPartObjects + static_cast<std::ptrdiff_t>(i) * kAnimationPartStride;
        if (pointer_field(animation, offset) == object) return true;
    }
    return false;
}

std::optional<bool> requested_crouch(const void* character) noexcept {
    if (!character) return std::nullopt;
    return read_field<std::uint8_t>(character, kCharacterRequestedCrouch) != 0;
}

} // namespace omvr::game::hands
