// OpenMotherVR-Provenance: clean
#include "loader/tracing.h"

#include "loader/context.h"

#include <string_view>

namespace omvr::loader {

#if defined(OMVR_TRACE)
namespace {

constexpr std::string_view kTracingLog =
    "trace: Tracy client started (traced build, localhost only); connect tracy-profiler 0.14.1";

} // namespace
#endif

void start_tracing() noexcept {
#if defined(OMVR_TRACE)
    auto& ctx = context();
    ctx.tracy.start();
    ctx.tracer.name_thread("game");
    ctx.log.line(kTracingLog);
#endif
}

} // namespace omvr::loader
