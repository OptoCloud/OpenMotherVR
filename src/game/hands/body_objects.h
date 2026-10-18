// OpenMotherVR-Provenance: clean
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

// The game objects the body features read (spec 21 §2.2, §2.3): the
// player character's animation component, its pose and skeleton, and its
// requested crouch byte. Thin accessors over game memory; no decisions.
namespace omvr::game::hands {

// ---- Layouts (21 §2.2, §2.3) ----------------------------------------------------

// The character's animation component, which FinalPoseStep runs on.
inline constexpr std::ptrdiff_t kCharacterAnimation = 0x3F0;
// The player's requested crouch, a byte (CharacterRequestCrouch, Stand).
inline constexpr std::ptrdiff_t kCharacterRequestedCrouch = 0x3E3A;
// The animation component's pose (pointer).
inline constexpr std::ptrdiff_t kAnimationPose = 0x24C;
// The render objects FinalPoseStep hands the pose: the model's one object,
// or six part objects 12 bytes apart (21 §2.3).
inline constexpr std::ptrdiff_t kAnimationModelObject = 0x3D8;
inline constexpr std::ptrdiff_t kAnimationPartObjects = 0x3E4;
inline constexpr std::ptrdiff_t kAnimationPartStride  = 0x0C;
inline constexpr std::size_t kAnimationPartCount      = 6;
// The Havok pose: its skeleton, and the model-space bone transforms (array
// data and size).
inline constexpr std::ptrdiff_t kPoseSkeleton   = 0x00;
inline constexpr std::ptrdiff_t kPoseModelData  = 0x10;
inline constexpr std::ptrdiff_t kPoseModelCount = 0x14;
// The Havok skeleton: its name, the parent indices (int16 array data) and
// the bones (8 bytes each, the name first; array data and size).
inline constexpr std::ptrdiff_t kSkeletonName       = 0x08;
inline constexpr std::ptrdiff_t kSkeletonParents    = 0x0C;
inline constexpr std::ptrdiff_t kSkeletonBones      = 0x18;
inline constexpr std::ptrdiff_t kSkeletonBoneCount  = 0x1C;
inline constexpr std::ptrdiff_t kSkeletonBoneStride = 0x08;
// Names are Havok string pointers: the lowest bit is an ownership flag.
inline constexpr std::uintptr_t kStringPointerFlags = 1;
// A bone or skeleton name is read up to this many characters.
inline constexpr std::size_t kMaxBoneName = 64;
// The most bones read from a skeleton (the stock first-person ones have
// about seventy); the loader checks it equals what body visibility plans.
inline constexpr std::size_t kMaxSkeletonBones = 256;

// The player's animation component, its pose and the pose's skeleton.
struct PlayerPose {
    void* animation      = nullptr;
    const void* pose     = nullptr;
    const void* skeleton = nullptr;
};

// The pose and skeleton of `character`'s animation component, or none.
std::optional<PlayerPose> player_pose(void* character) noexcept;

// The skeleton's name (empty when it has none).
std::string_view skeleton_name(const void* skeleton) noexcept;

// The skeleton's bones read into fixed arrays: names (views into the
// game's strings, valid while the skeleton lives) and parent indices.
struct SkeletonRead {
    std::array<std::string_view, kMaxSkeletonBones> names{};
    std::array<std::int16_t, kMaxSkeletonBones> parents{};
    std::size_t count = 0;
};

// What read_skeleton found.
enum class SkeletonReadResult : std::uint8_t {
    Ok,
    NoBones,      // the bone count is not above 0
    NoArrays,     // the bone or parent array pointer is null
    TooManyBones, // more than kMaxSkeletonBones
    UnnamedBone,  // a bone without a name
};

// Reads the skeleton into `out` (its bones when the result is Ok).
// `raw_count`: the bone count as read; `unnamed`: with UnnamedBone, the
// first bone without a name.
struct SkeletonReadInfo {
    SkeletonReadResult result = SkeletonReadResult::Ok;
    std::int32_t raw_count    = 0;
    std::size_t unnamed       = 0;
};
SkeletonReadInfo read_skeleton(const void* skeleton, SkeletonRead& out) noexcept;

// The pose's model-space bone transforms (the array FinalPoseStep hands the
// render objects, 21 §2.3) and their count. Job thread of the pose's step.
const void* pose_model_data(const void* pose) noexcept;
const void* pose_skeleton(const void* pose) noexcept;
std::int32_t pose_model_count(const void* pose) noexcept;

// `object` is one of the render objects of `animation` (21 §2.3).
bool is_model_object(const void* animation, const void* object) noexcept;

// The player's requested crouch byte (21 §2.2), or none without a player.
std::optional<bool> requested_crouch(const void* character) noexcept;

} // namespace omvr::game::hands
