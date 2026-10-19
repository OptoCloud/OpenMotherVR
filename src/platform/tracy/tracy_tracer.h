// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/tracer.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace omvr::platform::tracy {

// core::diag::ports::Tracer over the Tracy client (the traced build flavour
// only). The client is built with manual lifetime: nothing runs, and every
// call is a no-op, until start() has been called once, outside the loader
// lock. It is never shut down: like the Context that owns it, it lives
// until the process ends.
class TracyTracer final : public core::diag::ports::Tracer {
public:
    // Starts the client's threads and its listener (localhost only). Call
    // once, after attach, on any thread except under the loader lock.
    // Later calls do nothing.
    void start() noexcept;
    bool started() const noexcept { return started_.load(std::memory_order_acquire); }

    core::diag::ports::ZoneToken zone_begin(const core::diag::ZoneSite& site) noexcept override;
    void zone_end(const core::diag::ports::ZoneToken& token) noexcept override;
    void frame_mark(const char* name) noexcept override;
    void plot(const char* name, double value) noexcept override;
    void message(std::string_view text) noexcept override;
    void name_thread(const char* name) noexcept override;

    // gpu_zones.cpp. Render thread only.
    void gpu_attach(std::uintptr_t device, std::uintptr_t context) noexcept override;
    void gpu_detach() noexcept override;
    core::diag::ports::ZoneToken gpu_zone_begin(const core::diag::ZoneSite& site) noexcept override;
    void gpu_zone_end(const core::diag::ports::ZoneToken& token) noexcept override;
    void gpu_collect() noexcept override;

private:
    std::atomic<bool> started_{false};

    // GPU state, render thread only. The Tracy D3D11 context is created at
    // the first collect after both start() and gpu_attach (it creates its
    // query pool and calibrates the clocks: a one-time stall). Open zones
    // live in a small LIFO pool of slots, one Tracy zone scope each.
    static constexpr std::size_t kGpuDepth     = 8;
    static constexpr std::size_t kGpuSlotBytes = 32;
    std::uintptr_t gpu_device_  = 0;
    std::uintptr_t gpu_context_ = 0;
    void* gpu_ctx_              = nullptr; // tracy::D3D11Ctx*
    std::size_t gpu_open_       = 0;
    alignas(8) std::byte gpu_slots_[kGpuDepth][kGpuSlotBytes]{};
};

} // namespace omvr::platform::tracy
