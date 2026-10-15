#pragma once

#include "core/diag/ports/clock.h"

#include <cstddef>
#include <cstdint>

namespace omvr::core::diag {

// Writes the line prefix `[yyyy-MM-dd hh:mm:ss.fff pid=PPPPP] ` for `t` and
// `pid` into `out` (`cap` bytes, not terminated). Returns its length.
int format_prefix(char* out, std::size_t cap, const ports::LocalTime& t, std::uint32_t pid) noexcept;

} // namespace omvr::core::diag
