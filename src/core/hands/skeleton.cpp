// OpenMotherVR-Provenance: clean
#include "core/hands/skeleton.h"

namespace omvr::core::hands {

namespace {

constexpr char kNamespaceSeparator = ':';

} // namespace

std::string_view bare_bone_name(std::string_view name) noexcept {
    const auto colon = name.rfind(kNamespaceSeparator);
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

std::optional<std::int16_t> find_bone(const SkeletonBones& s, std::string_view prefix,
                                      std::string_view suffix) noexcept {
    for (std::size_t i = 0; i < s.names.size(); ++i) {
        const std::string_view n = bare_bone_name(s.names[i]);
        if (n.size() == prefix.size() + suffix.size() && n.starts_with(prefix) && n.ends_with(suffix))
            return static_cast<std::int16_t>(i);
    }
    return std::nullopt;
}

std::string_view to_string(SkeletonFault f) noexcept {
    switch (f) {
        case SkeletonFault::Unknown: return "no reason given";
        case SkeletonFault::NoBones: return "no bones";
        case SkeletonFault::NoArrays: return "no bone or parent array";
        case SkeletonFault::TooManyBones: return "too many bones";
        case SkeletonFault::UnnamedBone: return "a bone without a name";
        case SkeletonFault::TableMismatch: return "not the first-person bone table's skeleton";
    }
    return "?";
}

} // namespace omvr::core::hands
