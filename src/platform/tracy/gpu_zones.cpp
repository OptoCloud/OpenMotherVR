// OpenMotherVR-Provenance: clean
// The GPU half of TracyTracer: Tracy's D3D11 timestamp-query context. D3D11
// through the interfaces the game's device hands out only (AGENTS.md: never
// link d3d11.lib); TracyD3D11.hpp needs no import library.
#include "platform/tracy/tracy_tracer.h"

#include <tracy/TracyD3D11.hpp>

#include <cstring>
#include <new>

namespace omvr::platform::tracy {
namespace {

using core::diag::ZoneSite;
using core::diag::ports::ZoneToken;
using Scope = ::tracy::D3D11ZoneScope;

static_assert(sizeof(ZoneSite) == sizeof(::tracy::SourceLocationData));
static_assert(offsetof(ZoneSite, name) == offsetof(::tracy::SourceLocationData, name));
static_assert(offsetof(ZoneSite, line) == offsetof(::tracy::SourceLocationData, line));
static_assert(offsetof(ZoneSite, color) == offsetof(::tracy::SourceLocationData, color));

::tracy::D3D11Ctx* as_ctx(void* p) noexcept { return static_cast<::tracy::D3D11Ctx*>(p); }

// A GPU zone's token holds its slot number plus one; 0 is "no zone".
ZoneToken slot_token(std::uint32_t slot_plus_one) noexcept {
    ZoneToken token{};
    std::memcpy(token.storage, &slot_plus_one, sizeof(slot_plus_one));
    return token;
}

std::uint32_t token_slot(const ZoneToken& token) noexcept {
    std::uint32_t v = 0;
    std::memcpy(&v, token.storage, sizeof(v));
    return v;
}

} // namespace

void TracyTracer::gpu_attach(std::uintptr_t device, std::uintptr_t context) noexcept {
    gpu_detach();
    gpu_device_  = device;
    gpu_context_ = context;
}

// The context holds references on the device and its immediate context:
// it is released here so a device reset can release the game's device.
void TracyTracer::gpu_detach() noexcept {
    if (gpu_open_ != 0) return; // never under an open zone; the caller detaches between frames
    if (gpu_ctx_ != nullptr) {
        ::tracy::DestroyD3D11Context(as_ctx(gpu_ctx_));
        gpu_ctx_ = nullptr;
    }
    gpu_device_  = 0;
    gpu_context_ = 0;
}

ZoneToken TracyTracer::gpu_zone_begin(const ZoneSite& site) noexcept {
    static_assert(sizeof(Scope) <= kGpuSlotBytes && alignof(Scope) <= 8);
    if (gpu_ctx_ == nullptr || gpu_open_ == kGpuDepth) return ZoneToken{};
    new (gpu_slots_[gpu_open_])
        Scope(as_ctx(gpu_ctx_), reinterpret_cast<const ::tracy::SourceLocationData*>(&site), true);
    ++gpu_open_;
    return slot_token(static_cast<std::uint32_t>(gpu_open_));
}

void TracyTracer::gpu_zone_end(const ZoneToken& token) noexcept {
    const std::uint32_t slot_plus_one = token_slot(token);
    // Zones close in reverse order (they are scopes); anything else is a
    // zone that was never opened, or a bug, and is left alone.
    if (slot_plus_one == 0 || slot_plus_one != gpu_open_) return;
    --gpu_open_;
    std::launder(reinterpret_cast<Scope*>(gpu_slots_[gpu_open_]))->~Scope();
}

void TracyTracer::gpu_collect() noexcept {
    if (!started() || gpu_device_ == 0) return;
    if (gpu_ctx_ == nullptr) {
        gpu_ctx_ = ::tracy::CreateD3D11Context(reinterpret_cast<ID3D11Device*>(gpu_device_),
                                               reinterpret_cast<ID3D11DeviceContext*>(gpu_context_));
        static constexpr char kName[] = "game device";
        as_ctx(gpu_ctx_)->Name(kName, sizeof(kName) - 1);
        return;
    }
    as_ctx(gpu_ctx_)->Collect();
}

} // namespace omvr::platform::tracy
