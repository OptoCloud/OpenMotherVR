// OpenMotherVR-Provenance: clean
#include "core/vr/runtime_failure.h"

#include <utility>

namespace omvr::core::vr {

void RuntimeFailure::report(std::string what, std::int32_t code) noexcept { last_ = {std::move(what), code}; }

} // namespace omvr::core::vr
