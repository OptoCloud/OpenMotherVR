#include "game/stereo/cull.h"

namespace omvr::game::stereo {

namespace {

using CullFn = bool (__cdecl*)(const void*, const void*, const void*);
using DistanceFn = bool (__cdecl*)(const void*, const float*, float);

// R: stereo (A3): a member of the object that owns the fix.
CullTargets g_cull{};

} // namespace

void install_cull_fix(const CullTargets& targets) noexcept { g_cull = targets; }

bool call_cull_distance(const void* bounds, const float* camera, float scale) noexcept {
    return reinterpret_cast<DistanceFn>(g_cull.distance)(bounds, camera, scale);
}

bool call_cull_occlusion(const void* bounds, const void* ctx, const void* buffer) noexcept {
    return reinterpret_cast<CullFn>(g_cull.occlusion)(bounds, ctx, buffer);
}

} // namespace omvr::game::stereo
