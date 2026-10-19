// OpenMotherVR-Provenance: clean
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

// The player's skeleton as the hand and body features read it (spec 21
// §2.3, §10.2): what the game side found, its bones by name and parent, and
// why it could not be read. Shared by the arm pose edit and body
// visibility, so neither depends on the other.
namespace omvr::core::hands {

// The most bones a first-person skeleton may have for the arm edit and the
// body plan (the stock ones have about seventy, 21 §2.3); a larger one is
// left as the game poses it and is logged.
inline constexpr std::size_t kMaxBodyBones = 256;

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

// The first bone whose bare name is <prefix><suffix>, or none.
std::optional<std::int16_t> find_bone(const SkeletonBones& s, std::string_view prefix,
                                      std::string_view suffix) noexcept;
// The first bone whose bare name is `bare_name`, or none.
inline std::optional<std::int16_t> find_bone(const SkeletonBones& s, std::string_view bare_name) noexcept {
    return find_bone(s, bare_name, {});
}

// The player's skeleton as the game thread found it this frame.
struct PlayerSkeleton {
    std::uintptr_t animation = 0;
    std::uintptr_t pose      = 0; // 0: no player pose
    std::uintptr_t skeleton  = 0;
    std::string_view name; // the skeleton's own name, for the log
};

// Why the game side could not read the player's skeleton (21 §10.2, §18),
// for the log.
enum class SkeletonFault : std::uint8_t {
    Unknown,      // not said
    NoBones,      // the bone count is not above 0
    NoArrays,     // the bone or parent array pointer is null
    TooManyBones, // more bones than the reader holds
    UnnamedBone,  // a bone without a name
    // A skeleton of a first-person table's name whose bone count or parents
    // are not the table's (21 §2.3, r10): rejected, not posed with the
    // table's bone meanings.
    TableMismatch,
};
struct SkeletonReadFault {
    SkeletonFault fault = SkeletonFault::Unknown;
    std::int32_t count  = 0; // the skeleton's bone count as read
    // UnnamedBone: the first bone without a name; TableMismatch: the first
    // index whose parent differs from the table's.
    std::size_t bone = 0;
};

} // namespace omvr::core::hands
