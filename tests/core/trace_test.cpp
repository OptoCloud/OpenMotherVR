// OpenMotherVR-Provenance: clean
// core/diag/trace.h in the traced flavour: zones open and close on the
// tracer in scope order, a TracerRef without a tracer records nothing, and
// plots, frame marks and messages reach the tracer unchanged.
// (trace_off_test.cpp covers the release flavour.)

#define OMVR_TRACE 1
#include "core/diag/trace.h"

#include <doctest/doctest.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

using omvr::core::diag::TracerRef;
using omvr::core::diag::ZoneSite;
namespace ports = omvr::core::diag::ports;

// Records every call as a line, and numbers zones so the token round trip
// can be checked.
class RecordingTracer final : public ports::Tracer {
public:
    std::vector<std::string> events;

    ports::ZoneToken zone_begin(const ZoneSite& site) noexcept override {
        events.push_back(std::string("begin ") + site.name);
        return token_for(++next_);
    }
    void zone_end(const ports::ZoneToken& token) noexcept override {
        events.push_back("end " + std::to_string(number_of(token)));
    }
    void frame_mark(const char* name) noexcept override {
        events.push_back(std::string("frame ") + (name == nullptr ? "main" : name));
    }
    void plot(const char* name, double value) noexcept override {
        events.push_back(std::string("plot ") + name + " " + std::to_string(value));
    }
    void message(std::string_view text) noexcept override { events.push_back("message " + std::string(text)); }
    void name_thread(const char* name) noexcept override { events.push_back(std::string("thread ") + name); }
    void gpu_attach(std::uintptr_t device, std::uintptr_t context) noexcept override {
        events.push_back("gpu attach " + std::to_string(device) + " " + std::to_string(context));
    }
    void gpu_detach() noexcept override { events.push_back("gpu detach"); }
    ports::ZoneToken gpu_zone_begin(const ZoneSite& site) noexcept override {
        events.push_back(std::string("gpu begin ") + site.name);
        return token_for(++next_);
    }
    void gpu_zone_end(const ports::ZoneToken& token) noexcept override {
        events.push_back("gpu end " + std::to_string(number_of(token)));
    }
    void gpu_collect() noexcept override { events.push_back("gpu collect"); }

private:
    static ports::ZoneToken token_for(int n) {
        ports::ZoneToken t{};
        std::memcpy(t.storage, &n, sizeof(n));
        return t;
    }
    static int number_of(const ports::ZoneToken& t) {
        int n = 0;
        std::memcpy(&n, t.storage, sizeof(n));
        return n;
    }
    int next_ = 0;
};

void nested(TracerRef tracer) {
    OMVR_ZONE(tracer, "outer");
    {
        OMVR_ZONE(tracer, "inner");
    }
}

TEST_CASE("traced: the flavour is reported as built") { CHECK(omvr::core::diag::kTraceBuilt); }

TEST_CASE("traced: zones close in reverse order with their own tokens") {
    RecordingTracer tracer;
    nested(tracer);
    CHECK(tracer.events == std::vector<std::string>{"begin outer", "begin inner", "end 2", "end 1"});
}

TEST_CASE("traced: a zone's site carries the function, file and line") {
    struct SiteTracer final : ports::Tracer {
        const ZoneSite* seen = nullptr;
        ports::ZoneToken zone_begin(const ZoneSite& site) noexcept override {
            seen = &site;
            return {};
        }
        void zone_end(const ports::ZoneToken&) noexcept override {}
        void frame_mark(const char*) noexcept override {}
        void plot(const char*, double) noexcept override {}
        void message(std::string_view) noexcept override {}
        void name_thread(const char*) noexcept override {}
        void gpu_attach(std::uintptr_t, std::uintptr_t) noexcept override {}
        void gpu_detach() noexcept override {}
        ports::ZoneToken gpu_zone_begin(const ZoneSite&) noexcept override { return {}; }
        void gpu_zone_end(const ports::ZoneToken&) noexcept override {}
        void gpu_collect() noexcept override {}
    } tracer;
    const int line = __LINE__ + 1;
    { OMVR_ZONE(tracer, "here"); }
    REQUIRE(tracer.seen != nullptr);
    CHECK(std::string(tracer.seen->name) == "here");
    CHECK(tracer.seen->line == static_cast<std::uint32_t>(line));
    CHECK(std::string(tracer.seen->file).find("trace_test.cpp") != std::string::npos);
    CHECK(std::string(tracer.seen->function).find("DOCTEST") != std::string::npos);
    CHECK(tracer.seen->color == 0u);
}

TEST_CASE("traced: a TracerRef without a tracer records nothing and does not crash") {
    TracerRef none;
    CHECK(none.get() == nullptr);
    nested(none);
    none.frame_mark();
    none.plot("x", 1.0);
    none.message("m");
    none.name_thread("t");
    none.gpu_attach(1, 2);
    none.gpu_collect();
    none.gpu_detach();
    { OMVR_GPU_ZONE(none, "gpu"); }
}

TEST_CASE("traced: plots, frames, messages and thread names pass through") {
    RecordingTracer tracer;
    const TracerRef ref{tracer};
    ref.frame_mark();
    ref.frame_mark("game");
    ref.plot("pose_age", 2.5);
    ref.message("recentred");
    ref.name_thread("render");
    CHECK(tracer.events == std::vector<std::string>{"frame main", "frame game", "plot pose_age 2.500000",
                                                    "message recentred", "thread render"});
}

TEST_CASE("traced: GPU zones nest inside attach and collect") {
    RecordingTracer tracer;
    const TracerRef ref{tracer};
    ref.gpu_attach(10, 20);
    {
        OMVR_GPU_ZONE(ref, "game_render");
        OMVR_GPU_ZONE(ref, "mirror");
    }
    ref.gpu_collect();
    ref.gpu_detach();
    CHECK(tracer.events == std::vector<std::string>{"gpu attach 10 20", "gpu begin game_render", "gpu begin mirror",
                                                    "gpu end 2", "gpu end 1", "gpu collect", "gpu detach"});
}

} // namespace
