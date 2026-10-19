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

} // namespace omvr::core::hands
