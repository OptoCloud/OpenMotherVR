// OpenMotherVR-Provenance: clean
#pragma once

// The game's object culling tests (08 §2). Which tests exist and why the
// stereo device passes them is at cull_distance in
// loader/hooks/render_fix_hooks.cpp.
namespace omvr::game::stereo {

// The AI.exe functions the culling fix calls, found by the installer.
struct CullTargets {
    void* distance;  // bool __cdecl (const bounds*, const vec3* camera, float scale)
    void* occlusion; // bool __cdecl (const bounds*, const ctx*, const buffer*)
};

// The game's distance/LOD test and its occlusion-buffer test, through
// `targets` (set once at attach, before the calls are redirected). Job
// threads: direct calls, no indirection beyond the game's own function
// pointer.
bool call_cull_distance(const CullTargets& targets, const void* bounds, const float* camera,
                        float scale) noexcept;
bool call_cull_occlusion(const CullTargets& targets, const void* bounds, const void* ctx,
                         const void* buffer) noexcept;

} // namespace omvr::game::stereo
