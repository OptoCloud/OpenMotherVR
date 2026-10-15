#include "core/vr/runtime_failure.h"

#include <utility>

namespace omvr::core::vr {

namespace {

// R: vr (A1): a member of the object that reports the failure.
init_failure g_failure;

} // namespace

void set_init_failure(std::string what, int code) noexcept {
    g_failure = {std::move(what), code};
}

const init_failure& last_init_failure() noexcept {
    return g_failure;
}

} // namespace omvr::core::vr
