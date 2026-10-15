#pragma once

// The game's object culling tests.
namespace omvr::game::stereo {

// ---- Object culling (not in the original) ----------------------------------
//
// The game's per-object visibility loop (0xa1fd20, run on job threads)
// tests each object against a distance/LOD limit, the view frustum, and,
// where its cull context has them, two optional tests. Which POV culls
// decides which of those exist (POV +0x314 is its render-list group,
// CreatePOV's second argument):
//   group 2, BUILD_MAIN_RENDER_LIST: the main POV, the only one whose +0x310
//     holds the software occlusion buffer (ctx+0x5c), drawn from the single
//     centre view. In stereo it drops objects one eye still sees.
//   group 1, BUILD_SHADOWS_RENDER_LISTS: the shadow lights (POV type 2),
//     the only ones that get a view volume (ctx+0x60); the game's test runs.
//   group 0, BUILD_PRE_MAIN_RENDER_LISTS: the reflection POV, neither test.
// So the occlusion replacement below only ever sees main-POV culling, and
// no phase global is needed. The distance limit (scaled from the FOV)
// makes small props pop in and out, in every group.
struct CullTargets {
    void* distance;     // bool __cdecl (const bounds*, const vec3* camera, float scale)
    void* occlusion;    // bool __cdecl (const bounds*, const ctx*, const buffer*)
};
void install_cull_fix(const CullTargets& targets) noexcept;

// The game's distance/LOD test and its occlusion-buffer test.
bool call_cull_distance(const void* bounds, const float* camera, float scale) noexcept;
bool call_cull_occlusion(const void* bounds, const void* ctx, const void* buffer) noexcept;

} // namespace omvr::game::stereo
