// OpenMotherVR-Provenance: clean
// core/diag/trace.h in the release flavour (OMVR_TRACE not defined): a
// TracerRef holds nothing, every call is a no-op, and OMVR_ZONE does not
// evaluate its tracer. (trace_test.cpp covers the traced flavour.)

#undef OMVR_TRACE
#include "core/diag/trace.h"

#include <doctest/doctest.h>

#include <type_traits>

namespace {

using omvr::core::diag::TracerRef;

static_assert(std::is_empty_v<TracerRef>, "the release TracerRef carries no state");
static_assert(!omvr::core::diag::kTraceBuilt);

int evaluations = 0;
TracerRef counted() {
    ++evaluations;
    return {};
}

TEST_CASE("release: zones do not evaluate their tracer") {
    evaluations = 0;
    {
        OMVR_ZONE(counted(), "zone");
        OMVR_GPU_ZONE(counted(), "gpu");
    }
    CHECK(evaluations == 0);
}

TEST_CASE("release: TracerRef calls are no-ops") {
    const TracerRef ref;
    ref.frame_mark();
    ref.plot("x", 1.0);
    ref.message("m");
    ref.name_thread("t");
    ref.gpu_attach(1, 2);
    ref.gpu_collect();
    ref.gpu_detach();
    CHECK(ref.get() == nullptr);
}

} // namespace
