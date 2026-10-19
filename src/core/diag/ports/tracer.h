// OpenMotherVR-Provenance: clean
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace omvr::core::diag {

// Where a traced zone is in the source: static data, one per zone, so a
// zone costs no allocation. The layout is the profiler's own source
// location record (platform/tracy checks it), so the tracer passes it on
// as it is.
struct ZoneSite {
    const char* name;     // the zone's name in the profiler
    const char* function; // __func__
    const char* file;     // __FILE__
    std::uint32_t line;   // __LINE__
    std::uint32_t color;  // 0xRRGGBB, 0 for the profiler's default
};

namespace ports {

// The profiler's handle for an open zone, opaque to core. Big enough for
// what the tracer needs to close the zone again (platform/tracy checks it).
struct ZoneToken {
    alignas(8) std::byte storage[16];
};

// A timeline profiler (the traced build flavour only; the release build has
// none, see core/diag/trace.h). Any thread, no allocation by the caller.
// Names passed to frame_mark and plot must be string literals or other
// static strings: the profiler keys them by address.
class Tracer {
public:
    virtual ZoneToken zone_begin(const ZoneSite& site) noexcept = 0;
    virtual void zone_end(const ZoneToken& token) noexcept      = 0;
    // The end of a frame: nullptr for the main frame set, else the static
    // name of a separate set (the game thread's frame, for example).
    virtual void frame_mark(const char* name) noexcept = 0;
    // One sample of a plotted value.
    virtual void plot(const char* name, double value) noexcept = 0;
    // A line on the timeline; copied, so `text` may be temporary.
    virtual void message(std::string_view text) noexcept = 0;
    // The calling thread's name in the profiler; static string.
    virtual void name_thread(const char* name) noexcept = 0;

    // GPU time, render thread only. `device` and `context` are the game's
    // D3D11 device and immediate context (core::vr::GpuDevice and
    // GpuContext as integers); attach again after the device changes,
    // detach before it is released. Zones nest and close in reverse order
    // (ScopedGpuZone does both); collect once per frame, after the last.
    virtual void gpu_attach(std::uintptr_t device, std::uintptr_t context) noexcept = 0;
    virtual void gpu_detach() noexcept                                              = 0;
    virtual ZoneToken gpu_zone_begin(const ZoneSite& site) noexcept                 = 0;
    virtual void gpu_zone_end(const ZoneToken& token) noexcept                      = 0;
    virtual void gpu_collect() noexcept                                             = 0;

protected:
    ~Tracer() = default;
};

} // namespace ports
} // namespace omvr::core::diag
