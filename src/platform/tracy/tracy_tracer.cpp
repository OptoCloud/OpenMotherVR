// OpenMotherVR-Provenance: clean
#include "platform/tracy/tracy_tracer.h"

#include <tracy/TracyC.h>

#include <cstddef>
#include <cstring>

#if !defined(TRACY_ENABLE) || !defined(TRACY_MANUAL_LIFETIME) || !defined(TRACY_DELAYED_INIT)
#error "platform/tracy needs the project's Tracy port (ports/tracy): TRACY_ENABLE, TRACY_MANUAL_LIFETIME, TRACY_DELAYED_INIT"
#endif

namespace omvr::platform::tracy {
namespace {

using core::diag::ZoneSite;
using core::diag::ports::ZoneToken;

// ZoneSite is handed to Tracy as its own source location record.
static_assert(sizeof(ZoneSite) == sizeof(___tracy_source_location_data));
static_assert(offsetof(ZoneSite, name) == offsetof(___tracy_source_location_data, name));
static_assert(offsetof(ZoneSite, function) == offsetof(___tracy_source_location_data, function));
static_assert(offsetof(ZoneSite, file) == offsetof(___tracy_source_location_data, file));
static_assert(offsetof(ZoneSite, line) == offsetof(___tracy_source_location_data, line));
static_assert(offsetof(ZoneSite, color) == offsetof(___tracy_source_location_data, color));

static_assert(sizeof(TracyCZoneCtx) <= sizeof(ZoneToken));
static_assert(alignof(TracyCZoneCtx) <= alignof(ZoneToken));

ZoneToken to_token(const TracyCZoneCtx& ctx) noexcept {
    ZoneToken token{};
    std::memcpy(token.storage, &ctx, sizeof(ctx));
    return token;
}

TracyCZoneCtx from_token(const ZoneToken& token) noexcept {
    TracyCZoneCtx ctx;
    std::memcpy(&ctx, token.storage, sizeof(ctx));
    return ctx;
}

} // namespace

void TracyTracer::start() noexcept {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    ___tracy_startup_profiler();
}

// Before start() the client has no profiler object: every call returns
// without touching it. A zone opened then is inactive (active == 0, which a
// zero-filled token is) and is not closed through Tracy either.
ZoneToken TracyTracer::zone_begin(const ZoneSite& site) noexcept {
    if (!started()) return ZoneToken{};
    return to_token(___tracy_emit_zone_begin(reinterpret_cast<const ___tracy_source_location_data*>(&site), 1));
}

void TracyTracer::zone_end(const ZoneToken& token) noexcept {
    const TracyCZoneCtx ctx = from_token(token);
    if (ctx.active == 0) return;
    ___tracy_emit_zone_end(ctx);
}

void TracyTracer::frame_mark(const char* name) noexcept {
    if (started()) ___tracy_emit_frame_mark(name);
}

void TracyTracer::plot(const char* name, double value) noexcept {
    if (started()) ___tracy_emit_plot(name, value);
}

void TracyTracer::message(std::string_view text) noexcept {
    if (started()) ___tracy_emit_logString(TracyMessageSeverityInfo, 0, 0, text.size(), text.data());
}

void TracyTracer::name_thread(const char* name) noexcept {
    if (started()) ___tracy_set_thread_name(name);
}

} // namespace omvr::platform::tracy
