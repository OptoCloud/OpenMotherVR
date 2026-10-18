// OpenMotherVR-Provenance: clean
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// The bone names of the player's first-person skeletons (spec 21 §2.3,
// r10), game data of the Steam 2015 build. The skeletons the game loads
// carry no bone names; the game finds its bones through a table of name
// hashes in ANIMATION.PAK, which resolves to the names below. FEMALEFP and
// SPACESUITFP have the same bones in the same order with the same parents.
// Header only, so the unit tests check it without the DLL.
namespace omvr::game::hands {

// The skeletons this table describes, by their own name (skeleton +0x08),
// matched exactly.
inline constexpr std::array<std::string_view, 2> kFirstPersonSkeletons = {"FEMALEFP", "SPACESUITFP"};

inline constexpr std::size_t kFirstPersonBoneCount = 69;

// Bone k's name; the root (0) and bone 68 have none in the game's files.
// clang-format off
inline constexpr std::array<std::string_view, kFirstPersonBoneCount> kFirstPersonBoneNames = {
    /*  0 */ "",
    /*  1 */ "Head", "Head1",
    /*  3 */ "LeftShoulder", "LeftArm", "LeftArmRoll", "LeftForeArm", "LeftForeArmRoll",
    /*  8 */ "LeftForeArmRoll1", "LeftHand",
    /* 10 */ "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3",
    /* 13 */ "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3",
    /* 16 */ "LeftHandPinky1", "LeftHandPinky2", "LeftHandPinky3",
    /* 19 */ "LeftHandRing1", "LeftHandRing2", "LeftHandRing3",
    /* 22 */ "LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3",
    /* 25 */ "LeftWeaponBone",
    /* 26 */ "RightShoulder", "RightArm", "RightArmRoll", "RightForeArm", "RightForeArmRoll",
    /* 31 */ "RightForeArmRoll1", "RightHand",
    /* 33 */ "RightHandIndex1", "RightHandIndex2", "RightHandIndex3",
    /* 36 */ "RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3",
    /* 39 */ "RightHandPinky1", "RightHandPinky2", "RightHandPinky3",
    /* 42 */ "RightHandRing1", "RightHandRing2", "RightHandRing3",
    /* 45 */ "RightHandThumb1", "RightHandThumb2", "RightHandThumb3",
    /* 48 */ "RightWeaponBone",
    /* 49 */ "Hips", "Pelvis",
    /* 51 */ "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase",
    /* 55 */ "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase",
    /* 59 */ "Spine", "Spine1", "Spine2", "Spine3", "Neck",
    /* 64 */ "L_Foot", "L_Hand", "R_Foot", "R_Hand", "",
};

// Bone k's parent index (-1: the root). The arms hang from Head, not from
// the spine.
inline constexpr std::array<std::int16_t, kFirstPersonBoneCount> kFirstPersonBoneParents = {
    /*  0 */ -1,
    /*  1 */ 0, 1,
    /*  3 */ 1, 3, 4, 4, 6, 6, 6,
    /* 10 */ 9, 10, 11, 9, 13, 14, 9, 16, 17, 9, 19, 20, 9, 22, 23, 9,
    /* 26 */ 1, 26, 27, 27, 29, 29, 29,
    /* 33 */ 32, 33, 34, 32, 36, 37, 32, 39, 40, 32, 42, 43, 32, 45, 46, 32,
    /* 49 */ 0, 49, 50, 51, 52, 53, 50, 55, 56, 57,
    /* 59 */ 49, 59, 60, 61, 62,
    /* 64 */ 0, 0, 0, 0, 0,
};
// clang-format on

// Whether a skeleton of this name is one the table describes.
constexpr bool uses_first_person_table(std::string_view skeleton_name) noexcept {
    for (const auto name : kFirstPersonSkeletons)
        if (name == skeleton_name) return true;
    return false;
}

// The first bone index at which a skeleton with `count` bones whose first
// parent indices are `parents` differs from the table: the first index in
// both whose parent differs, else the length of the shorter. kNoDifference
// when the count is the table's and every parent matches (21 §2.3).
inline constexpr std::size_t kNoDifference = static_cast<std::size_t>(-1);
constexpr std::size_t first_table_difference(std::size_t count,
                                             std::span<const std::int16_t> parents) noexcept {
    const std::size_t common = std::min({count, parents.size(), kFirstPersonBoneCount});
    for (std::size_t i = 0; i < common; ++i)
        if (parents[i] != kFirstPersonBoneParents[i]) return i;
    if (count == kFirstPersonBoneCount && parents.size() >= kFirstPersonBoneCount) return kNoDifference;
    return common;
}

} // namespace omvr::game::hands
