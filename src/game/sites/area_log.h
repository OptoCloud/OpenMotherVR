// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

#include <cstdint>
#include <string_view>

// The outcome lines of the patch areas (11).
namespace omvr::game::sites {

// Why an area is off on a build whose table has no address for it.
inline constexpr std::string_view kNoAddressWhy = "this build has no address for it";

// One outcome line per feature area at attach, so a log shows which parts of
// the mod are in place and, for those that are not, why (11).
void area_on(core::diag::ports::Log& log, std::string_view area, int sites) noexcept;
void area_off(core::diag::ports::Log& log, std::string_view area, std::string_view why) noexcept;

// True when this build has the area's address (`rva` is not 0); otherwise
// reports the area off for that reason.
bool known_or_off(core::diag::ports::Log& log, std::uint32_t rva, std::string_view area) noexcept;

} // namespace omvr::game::sites
