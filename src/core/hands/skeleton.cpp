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

} // namespace omvr::core::hands
