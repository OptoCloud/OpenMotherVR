#include "game/stereo/cull.h"

#include "game/code_address.h"

namespace omvr::game::stereo {

namespace {

using DistanceFn  = bool(__cdecl*)(const void* bounds, const float* camera, float scale);
using OcclusionFn = bool(__cdecl*)(const void* bounds, const void* ctx, const void* buffer);

} // namespace

bool call_cull_distance(const CullTargets& targets, const void* bounds, const float* camera,
                        float scale) noexcept {
    const DistanceFn distance = CodeAddress{targets.distance};
    return distance(bounds, camera, scale);
}

bool call_cull_occlusion(const CullTargets& targets, const void* bounds, const void* ctx,
                         const void* buffer) noexcept {
    const OcclusionFn occlusion = CodeAddress{targets.occlusion};
    return occlusion(bounds, ctx, buffer);
}

} // namespace omvr::game::stereo
