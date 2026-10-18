// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

// Body visibility (spec 21 §10): which bones of the player's first-person
// skeleton are hidden for each Body.Visible value, and how a hidden bone is
// collapsed in the copy of the pose the game hands its model's render
// objects (21 §2.3, §10.2). Pure: the game side reads the skeleton into
// names and parent indices and hands the pose in as bone transforms.
namespace omvr::core::hands {

// The most bones a first-person skeleton may have for the plan (the stock
// ones have about seventy, 21 §2.3); a larger one is not hidden and is
// logged.
inline constexpr std::size_t kMaxBodyBones = 256;
// The most nearest visible bones a hidden bone collapses towards (ties of
// 21 §10.2); further ties are dropped.
inline constexpr std::size_t kMaxCollapseAnchors = 4;

// The bone groups of 21 §2.3 as body visibility uses them. The weapon bones
// (`LeftWeaponBone`, `RightWeaponBone`, listed with the fingers) are their
// own group: held items hang there and are always drawn (21 §10.2).
enum class BoneGroup : std::uint8_t { Body, Shoulder, Arm, Hand, Weapon };

// A bone transform of the game's pose (21 §2.3): translation, rotation
// quaternion (x, y, z, w) and scale, four floats each.
struct BoneTransform {
    std::array<float, 4> translation{};
    std::array<float, 4> rotation{};
    std::array<float, 4> scale{};
};
inline constexpr std::size_t kBoneTransformSize = 0x30;
static_assert(sizeof(BoneTransform) == kBoneTransformSize);

// The skeleton as read from the game: each bone's name and its parent's
// index (-1 for a root), in bone order. The names are the skeleton's own,
// or those of the first-person bone table (21 §2.3, r10) for a skeleton
// that carries none and matched it by name, bone count and parents; there
// a bone the table does not name has an empty name.
struct SkeletonBones {
    std::span<const std::string_view> names;
    std::span<const std::int16_t> parents;
    bool names_from_table = false;
};

// The name without a namespace prefix: the game's tools write bones as
// `FemaleFP:LeftHand` (the skeletons' mirror files, 21 §2.3), the bone
// group files as `LeftHand`.
std::string_view bare_bone_name(std::string_view name) noexcept;

// The group a bone name is listed in (21 §2.3), or none for a bone the
// group files do not list (fingers, helper bones).
std::optional<BoneGroup> listed_group(std::string_view name) noexcept;

// Whether a group is drawn for a Body.Visible value (21 §10.2).
bool group_visible(BoneGroup g, config::BodyVisible v) noexcept;

// One bone's place in the plan: kept as the game posed it (no anchors), or
// collapsed at the mean of its anchors' translations.
struct BoneCollapse {
    std::uint8_t anchor_count = 0;
    std::array<std::int16_t, kMaxCollapseAnchors> anchors{};
};

enum class PlanResult : std::uint8_t {
    Ok,
    TooManyBones, // more than kMaxBodyBones, or the arrays disagree in size
    BadParents,   // a parent index out of range, or a loop
    MissingBones, // a bone the plan needs is not in the skeleton by name
};

// The bones the plan cannot do without (21 §4.6, §10.2: a skeleton that
// lacks them by name is not hidden): both shoulders, arms and hands, and
// the spine. The first missing one, for the log.
std::string_view first_missing_bone(const SkeletonBones& s) noexcept;

// Which bones are collapsed for one Body.Visible value of one skeleton.
struct CollapsePlan {
    std::array<BoneCollapse, kMaxBodyBones> bones{};
    std::size_t count  = 0; // bones in the skeleton
    std::size_t hidden = 0; // bones collapsed
};

// 21 §10.2: every bone takes the group of its name or, unlisted, of its
// nearest listed ancestor (none: Body). A bone whose group is not drawn is
// hidden; it collapses towards its nearest drawn bones by tree distance
// (all of them when several are equally near, up to kMaxCollapseAnchors):
// at the joint it shares with the hidden region, which is the drawn bone's
// own origin when the drawn bone hangs below the hidden ones, and the
// origin of the hidden bone below the drawn one otherwise. With no drawn
// bone at all, every bone collapses at the first root. So skin bound to
// hidden bones only shrinks into the joints where it meets the drawn
// parts, and skin shared with a drawn bone stays attached to it.
PlanResult build_collapse_plan(const SkeletonBones& s, config::BodyVisible v, CollapsePlan& out) noexcept;

// The pose the render objects receive: `src` as the game posed it, with
// every hidden bone of `plan` collapsed (identity rotation, scale
// kCollapseScale, translation the mean of its anchors' translations in
// `src`). `dst` must hold src.size() transforms; src.size() must equal
// plan.count. No allocation: the job thread calls it (21 §17).
void apply_collapse_plan(const CollapsePlan& plan, std::span<const BoneTransform> src,
                         std::span<BoneTransform> dst) noexcept;

} // namespace omvr::core::hands
