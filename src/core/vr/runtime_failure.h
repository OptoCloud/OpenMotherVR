#pragma once

#include <string>

namespace omvr::core::vr {

// Why the chosen runtime failed to start: the runtime's error text and code,
// set by the backend that failed, for the player-facing dialog.
struct init_failure {
    std::string what;
    int         code = 0;
};
void set_init_failure(std::string what, int code) noexcept;
const init_failure& last_init_failure() noexcept;

} // namespace omvr::core::vr
