// OpenMotherVR-Provenance: clean
#include "game/sites/area_log.h"

namespace omvr::game::sites {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kAreaOnLog  = "[hook] {}: in place on {} sites";
constexpr std::string_view kAreaOffLog = "[hook] {}: off; {}";

} // namespace

void area_on(ports::Log& log, std::string_view area, int sites) noexcept {
    ports::logf(log, kAreaOnLog, area, sites);
}

void area_off(ports::Log& log, std::string_view area, std::string_view why) noexcept {
    ports::logf(log, kAreaOffLog, area, why);
}

bool known_or_off(ports::Log& log, std::uint32_t rva, std::string_view area) noexcept {
    if (rva != 0) return true;
    area_off(log, area, kNoAddressWhy);
    return false;
}

} // namespace omvr::game::sites
