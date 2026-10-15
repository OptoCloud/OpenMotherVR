#include "loader/hooks/render_fix_hooks.h"

#include "core/stereo/cull_policy.h"
#include "game/stereo/cull.h"
#include "game/stereo/head_clip.h"
#include "loader/context.h"

namespace omvr::loader::hooks {

void __fastcall render_reflection(void* pov, void*) noexcept { context().reflections.render(pov); }

void __cdecl stereo_final_draw() noexcept { context().stereo_aa.final_draw(); }

void __cdecl run_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept {
    context().stereo_aa.run_smaa(color, second, temporal);
}

// Called from the game's job threads; StereoDevice::running() is atomic.
bool __cdecl cull_distance(const void* bounds, const float* camera, float scale) noexcept {
    return core::stereo::cull_test_passes_in_stereo(context().stereo.running()) ||
           game::stereo::call_cull_distance(bounds, camera, scale);
}

// Only reached while the main POV culls: `buffer` is the context's
// occlusion buffer, which no other POV has (see core/stereo/cull_policy.h).
bool __cdecl cull_occlusion(const void* bounds, const void* ctx, const void* buffer) noexcept {
    return core::stereo::cull_test_passes_in_stereo(context().stereo.running()) ||
           game::stereo::call_cull_occlusion(bounds, ctx, buffer);
}

float __stdcall head_clip_fade(const float* eye, const float* pivot) noexcept {
    const float fade = game::stereo::call_head_clip_fade(eye, pivot);
    return context().head_clip.apply(fade, context().stereo.running());
}

} // namespace omvr::loader::hooks
