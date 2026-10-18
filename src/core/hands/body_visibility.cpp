// OpenMotherVR-Provenance: clean
#include "core/hands/body_visibility.h"

#include "core/hands/tuning.h"

#include <algorithm>

namespace omvr::core::hands {

namespace {

using config::BodyVisible;

constexpr std::int16_t kNoParent   = -1;
constexpr char kNamespaceSeparator = ':';

struct ListedBone {
    std::string_view name;
    BoneGroup group;
};

// 21 §2.3: the bone group files of FEMALEFP and SPACESUITFP (the same bones
// in both). The fingers are not listed: they take the hand's group as its
// descendants.
constexpr std::array kListedBones = {
    ListedBone{"Hips", BoneGroup::Body},
    ListedBone{"Pelvis", BoneGroup::Body},
    ListedBone{"Spine", BoneGroup::Body},
    ListedBone{"Spine1", BoneGroup::Body},
    ListedBone{"Spine2", BoneGroup::Body},
    ListedBone{"Neck", BoneGroup::Body},
    ListedBone{"Head", BoneGroup::Body},
    ListedBone{"Head1", BoneGroup::Body},
    ListedBone{"LeftUpLeg", BoneGroup::Body},
    ListedBone{"LeftLeg", BoneGroup::Body},
    ListedBone{"LeftFoot", BoneGroup::Body},
    ListedBone{"LeftToeBase", BoneGroup::Body},
    ListedBone{"RightUpLeg", BoneGroup::Body},
    ListedBone{"RightLeg", BoneGroup::Body},
    ListedBone{"RightFoot", BoneGroup::Body},
    ListedBone{"RightToeBase", BoneGroup::Body},
    ListedBone{"LeftShoulder", BoneGroup::Shoulder},
    ListedBone{"RightShoulder", BoneGroup::Shoulder},
    ListedBone{"LeftArm", BoneGroup::Arm},
    ListedBone{"LeftArmRoll", BoneGroup::Arm},
    ListedBone{"LeftForeArm", BoneGroup::Arm},
    ListedBone{"LeftForeArmRoll", BoneGroup::Arm},
    ListedBone{"LeftForeArmRoll1", BoneGroup::Arm},
    ListedBone{"RightArm", BoneGroup::Arm},
    ListedBone{"RightArmRoll", BoneGroup::Arm},
    ListedBone{"RightForeArm", BoneGroup::Arm},
    ListedBone{"RightForeArmRoll", BoneGroup::Arm},
    ListedBone{"RightForeArmRoll1", BoneGroup::Arm},
    ListedBone{"LeftHand", BoneGroup::Hand},
    ListedBone{"RightHand", BoneGroup::Hand},
    ListedBone{"LeftWeaponBone", BoneGroup::Weapon},
    ListedBone{"RightWeaponBone", BoneGroup::Weapon},
};

// 21 §10.2: the bones a plan needs by name.
constexpr std::array<std::string_view, 7> kRequiredBones = {
    "Spine", "LeftShoulder", "RightShoulder", "LeftArm", "RightArm", "LeftHand", "RightHand",
};

bool has_bone(const SkeletonBones& s, std::string_view name) noexcept {
    return std::ranges::any_of(s.names, [&](std::string_view n) { return bare_bone_name(n) == name; });
}

// Every parent index is a bone or -1 and no bone is its own ancestor.
bool parents_valid(std::span<const std::int16_t> parents) noexcept {
    const auto n = static_cast<std::int32_t>(parents.size());
    for (std::int32_t i = 0; i < n; ++i) {
        std::int32_t p     = parents[static_cast<std::size_t>(i)];
        std::int32_t steps = 0;
        while (p != kNoParent) {
            if (p < 0 || p >= n || p == i || ++steps > n) return false;
            p = parents[static_cast<std::size_t>(p)];
        }
    }
    return true;
}

// Each bone's group: its own, else its nearest listed ancestor's, else Body.
void assign_groups(const SkeletonBones& s, std::span<BoneGroup> groups) noexcept {
    for (std::size_t i = 0; i < s.names.size(); ++i) {
        std::int32_t b = static_cast<std::int32_t>(i);
        groups[i]      = BoneGroup::Body;
        while (b != kNoParent) {
            if (const auto g = listed_group(s.names[static_cast<std::size_t>(b)])) {
                groups[i] = *g;
                break;
            }
            b = s.parents[static_cast<std::size_t>(b)];
        }
    }
}

// The nearest kept bones of hidden bone `start` by tree distance, each as
// the joint the collapse goes to (see build_collapse_plan). Breadth-first
// over the tree, fixed arrays only.
BoneCollapse nearest_anchors(const SkeletonBones& s, std::span<const bool> kept, std::size_t start) noexcept {
    const std::size_t n = s.parents.size();
    std::array<std::int16_t, kMaxBodyBones> came_from{};
    std::array<std::int16_t, kMaxBodyBones> queue{};
    std::array<std::uint16_t, kMaxBodyBones> depth{};
    std::array<bool, kMaxBodyBones> seen{};
    std::size_t head = 0, tail = 0;
    queue[tail++]    = static_cast<std::int16_t>(start);
    seen[start]      = true;
    came_from[start] = kNoParent;
    BoneCollapse out;
    std::uint16_t found_depth = 0;
    while (head < tail) {
        const auto b = static_cast<std::size_t>(queue[head++]);
        if (out.anchor_count > 0 && depth[b] > found_depth) break;
        if (kept[b]) {
            const auto from = came_from[b];
            // Reached by stepping up to the parent: the kept bone is an
            // ancestor, and the joint is the hidden child it was reached from.
            const bool up = from != kNoParent &&
                            s.parents[static_cast<std::size_t>(from)] == static_cast<std::int16_t>(b);
            if (out.anchor_count < kMaxCollapseAnchors)
                out.anchors[out.anchor_count++] = up ? from : static_cast<std::int16_t>(b);
            found_depth = depth[b];
            continue;
        }
        const auto visit = [&](std::size_t next) {
            if (seen[next]) return;
            seen[next]      = true;
            came_from[next] = static_cast<std::int16_t>(b);
            depth[next]     = static_cast<std::uint16_t>(depth[b] + 1);
            queue[tail++]   = static_cast<std::int16_t>(next);
        };
        if (const auto p = s.parents[b]; p != kNoParent) visit(static_cast<std::size_t>(p));
        for (std::size_t c = 0; c < n; ++c)
            if (s.parents[c] == static_cast<std::int16_t>(b)) visit(c);
    }
    return out;
}

std::size_t first_root(std::span<const std::int16_t> parents) noexcept {
    const auto it = std::ranges::find(parents, kNoParent);
    return it == parents.end() ? 0 : static_cast<std::size_t>(it - parents.begin());
}

// The root of bone `b`'s tree (the parents are valid: no loop).
std::size_t root_of(std::span<const std::int16_t> parents, std::size_t b) noexcept {
    while (parents[b] != kNoParent) b = static_cast<std::size_t>(parents[b]);
    return b;
}

} // namespace

std::string_view bare_bone_name(std::string_view name) noexcept {
    const auto colon = name.rfind(kNamespaceSeparator);
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

std::optional<BoneGroup> listed_group(std::string_view name) noexcept {
    const std::string_view bare = bare_bone_name(name);
    for (const auto& b : kListedBones)
        if (b.name == bare) return b.group;
    return std::nullopt;
}

bool group_visible(BoneGroup g, BodyVisible v) noexcept {
    switch (g) {
        case BoneGroup::Weapon: return true;
        case BoneGroup::Hand: return v != BodyVisible::None;
        case BoneGroup::Shoulder:
        case BoneGroup::Arm: return v == BodyVisible::Full || v == BodyVisible::ArmsAndHands;
        case BoneGroup::Body: return v == BodyVisible::Full;
    }
    return true;
}

std::string_view first_missing_bone(const SkeletonBones& s) noexcept {
    for (const auto name : kRequiredBones)
        if (!has_bone(s, name)) return name;
    return {};
}

PlanResult build_collapse_plan(const SkeletonBones& s, BodyVisible v, CollapsePlan& out) noexcept {
    out.count           = 0;
    out.hidden          = 0;
    const std::size_t n = s.names.size();
    if (n > kMaxBodyBones || s.parents.size() != n) return PlanResult::TooManyBones;
    if (!parents_valid(s.parents)) return PlanResult::BadParents;
    if (!first_missing_bone(s).empty()) return PlanResult::MissingBones;

    std::array<BoneGroup, kMaxBodyBones> groups{};
    assign_groups(s, std::span{groups}.first(n));
    std::array<bool, kMaxBodyBones> kept{};
    for (std::size_t i = 0; i < n; ++i) kept[i] = group_visible(groups[i], v);
    const bool any_kept = std::ranges::any_of(std::span{kept}.first(n), [](bool k) { return k; });

    out.count = n;
    for (std::size_t i = 0; i < n; ++i) {
        out.bones[i] = {};
        if (kept[i]) continue;
        ++out.hidden;
        if (!any_kept) {
            out.bones[i].anchor_count = 1;
            out.bones[i].anchors[0]   = static_cast<std::int16_t>(first_root(s.parents));
            continue;
        }
        out.bones[i] = nearest_anchors(s, std::span{kept}.first(n), i);
        // A tree of the skeleton with nothing drawn in it: at its root.
        if (out.bones[i].anchor_count == 0) {
            out.bones[i].anchor_count = 1;
            out.bones[i].anchors[0]   = static_cast<std::int16_t>(root_of(s.parents, i));
        }
    }
    return PlanResult::Ok;
}

void apply_collapse_plan(const CollapsePlan& plan, std::span<const BoneTransform> src,
                         std::span<BoneTransform> dst) noexcept {
    const std::size_t n = std::min({src.size(), dst.size(), plan.count});
    for (std::size_t i = 0; i < n; ++i) {
        const BoneCollapse& c = plan.bones[i];
        dst[i]                = src[i];
        if (c.anchor_count == 0) continue;
        std::array<float, 3> sum{};
        for (std::size_t a = 0; a < c.anchor_count; ++a) {
            const auto& t = src[static_cast<std::size_t>(c.anchors[a])].translation;
            for (std::size_t k = 0; k < sum.size(); ++k) sum[k] += t[k];
        }
        const float inv = 1.0f / static_cast<float>(c.anchor_count);
        for (std::size_t k = 0; k < sum.size(); ++k) {
            dst[i].translation[k] = sum[k] * inv;
            dst[i].rotation[k]    = 0.0f;
            dst[i].scale[k]       = kCollapseScale;
        }
        dst[i].rotation[3] = 1.0f;
    }
}

} // namespace omvr::core::hands
