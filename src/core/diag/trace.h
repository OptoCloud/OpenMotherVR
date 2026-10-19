// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/tracer.h"

#include <cstdint>
#include <string_view>

// Tracing for the traced build flavour (MOTHERVR_TRACY, which defines
// OMVR_TRACE). Code marks zones with OMVR_ZONE and holds a TracerRef where
// it plots or marks frames. In the release flavour both compile to nothing:
// TracerRef is empty, every call on it is an inline no-op, and OMVR_ZONE
// does not evaluate its tracer, so release code and its imports are
// unchanged.
//
// The two flavours' types live in different inline namespaces, so a
// translation unit built the other way (the tests build both) is a
// different type, not an ODR violation.
namespace omvr::core::diag {

#if defined(OMVR_TRACE)
inline namespace traced {

inline constexpr bool kTraceBuilt = true;

// A tracer, or none (nothing recorded). Copyable; the tracer outlives it.
class TracerRef {
public:
    constexpr TracerRef() noexcept = default;
    constexpr TracerRef(ports::Tracer& tracer) noexcept : tracer_(&tracer) {} // NOLINT(google-explicit-constructor)

    void frame_mark(const char* name = nullptr) const noexcept {
        if (tracer_ != nullptr) tracer_->frame_mark(name);
    }
    void plot(const char* name, double value) const noexcept {
        if (tracer_ != nullptr) tracer_->plot(name, value);
    }
    void message(std::string_view text) const noexcept {
        if (tracer_ != nullptr) tracer_->message(text);
    }
    void name_thread(const char* name) const noexcept {
        if (tracer_ != nullptr) tracer_->name_thread(name);
    }
    void gpu_attach(std::uintptr_t device, std::uintptr_t context) const noexcept {
        if (tracer_ != nullptr) tracer_->gpu_attach(device, context);
    }
    void gpu_detach() const noexcept {
        if (tracer_ != nullptr) tracer_->gpu_detach();
    }
    void gpu_collect() const noexcept {
        if (tracer_ != nullptr) tracer_->gpu_collect();
    }
    ports::Tracer* get() const noexcept { return tracer_; }

private:
    ports::Tracer* tracer_ = nullptr;
};

// Opens a CPU zone for its lifetime. Use through OMVR_ZONE.
class ScopedZone final {
public:
    ScopedZone(TracerRef tracer, const ZoneSite& site) noexcept : tracer_(tracer.get()) {
        if (tracer_ != nullptr) token_ = tracer_->zone_begin(site);
    }
    ~ScopedZone() {
        if (tracer_ != nullptr) tracer_->zone_end(token_);
    }
    ScopedZone(const ScopedZone&)            = delete;
    ScopedZone& operator=(const ScopedZone&) = delete;

private:
    ports::Tracer* tracer_;
    ports::ZoneToken token_{};
};

// Opens a GPU zone for its lifetime (render thread). Use through
// OMVR_GPU_ZONE.
class ScopedGpuZone final {
public:
    ScopedGpuZone(TracerRef tracer, const ZoneSite& site) noexcept : tracer_(tracer.get()) {
        if (tracer_ != nullptr) token_ = tracer_->gpu_zone_begin(site);
    }
    ~ScopedGpuZone() {
        if (tracer_ != nullptr) tracer_->gpu_zone_end(token_);
    }
    ScopedGpuZone(const ScopedGpuZone&)            = delete;
    ScopedGpuZone& operator=(const ScopedGpuZone&) = delete;

private:
    ports::Tracer* tracer_;
    ports::ZoneToken token_{};
};

} // namespace traced
#else
inline namespace untraced {

inline constexpr bool kTraceBuilt = false;

// The release flavour: no tracer, nothing recorded, no state.
class TracerRef {
public:
    constexpr TracerRef() noexcept = default;
    constexpr TracerRef(ports::Tracer&) noexcept {} // NOLINT(google-explicit-constructor)

    void frame_mark(const char* = nullptr) const noexcept {}
    void plot(const char*, double) const noexcept {}
    void message(std::string_view) const noexcept {}
    void name_thread(const char*) const noexcept {}
    void gpu_attach(std::uintptr_t, std::uintptr_t) const noexcept {}
    void gpu_detach() const noexcept {}
    void gpu_collect() const noexcept {}
    ports::Tracer* get() const noexcept { return nullptr; }
};

} // namespace untraced
#endif

} // namespace omvr::core::diag

#define OMVR_TRACE_CONCAT_INNER(a, b) a##b
#define OMVR_TRACE_CONCAT(a, b) OMVR_TRACE_CONCAT_INNER(a, b)

// A zone named `name` (a string literal) from here to the end of the
// enclosing block, recorded on `tracer` (a TracerRef or ports::Tracer&).
// A macro because each zone needs its own static ZoneSite with the
// caller's __FILE__ and __LINE__. In the release flavour `tracer` is
// checked by the compiler but not evaluated.
// OMVR_GPU_ZONE is the same for GPU time, on the render thread, between
// gpu_attach and gpu_detach.
#if defined(OMVR_TRACE)
#define OMVR_TRACE_SCOPE(scope, tracer, name)                                                                          \
    static const ::omvr::core::diag::ZoneSite OMVR_TRACE_CONCAT(omvr_zone_site_, __LINE__){                           \
        name, __func__, __FILE__, static_cast<std::uint32_t>(__LINE__), 0};                                            \
    const ::omvr::core::diag::scope OMVR_TRACE_CONCAT(omvr_zone_, __LINE__) {                                          \
        ::omvr::core::diag::TracerRef{tracer}, OMVR_TRACE_CONCAT(omvr_zone_site_, __LINE__)                            \
    }
#define OMVR_ZONE(tracer, name) OMVR_TRACE_SCOPE(ScopedZone, tracer, name)
#define OMVR_GPU_ZONE(tracer, name) OMVR_TRACE_SCOPE(ScopedGpuZone, tracer, name)
#else
#define OMVR_ZONE(tracer, name) static_cast<void>(sizeof(::omvr::core::diag::TracerRef{tracer}))
#define OMVR_GPU_ZONE(tracer, name) static_cast<void>(sizeof(::omvr::core::diag::TracerRef{tracer}))
#endif
