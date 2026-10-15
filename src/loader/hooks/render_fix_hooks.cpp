#include "loader/hooks/render_fix_hooks.h"

#include "core/stereo/cull_policy.h"
#include "core/stereo/reflection_pass.h"
#include "core/stereo/stereo_aa.h"
#include "game/stereo/cull.h"
#include "game/stereo/head_clip.h"
#include "game/stereo/postfx.h"
#include "game/stereo/renderer.h"
#include "game/stereo/stereo_objects.h"
#include "loader/context.h"
#include "loader/hooks/guard.h"
#include "platform/d3d11/render/texture_dump.h"

#include <d3d11.h>

#include <type_traits>

namespace omvr::loader::hooks {

namespace {

using core::stereo::AaMode;
using core::stereo::StereoAa;
using game::DeviceState;
using game::StereoInterface;

// The game's functions these replace (07 §5, 08 §2 to §4).
using ReflectionRenderFn = void(__fastcall*)(void* pov, void* edx);
using DrawFn             = void(__cdecl*)();
using SmaaFn             = void(__cdecl*)(void* color, void* second, const std::uint8_t* temporal);
using DistanceFn         = bool(__cdecl*)(const void* bounds, const float* camera, float scale);
using OcclusionFn        = bool(__cdecl*)(const void* bounds, const void* ctx, const void* buffer);
using HeadClipFadeFn     = float(__stdcall*)(const float* eye, const float* pivot);

static_assert(std::is_convertible_v<decltype(&render_reflection), ReflectionRenderFn>);
static_assert(std::is_convertible_v<decltype(&stereo_final_draw), DrawFn>);
static_assert(std::is_convertible_v<decltype(&run_smaa), SmaaFn>);
static_assert(std::is_convertible_v<decltype(&cull_distance), DistanceFn>);
static_assert(std::is_convertible_v<decltype(&cull_occlusion), OcclusionFn>);
static_assert(std::is_convertible_v<decltype(&head_clip_fade), HeadClipFadeFn>);

// Debug (F10): write what the reflection pass left bound as render target
// to <game dir>/mothervr_reflection_<eye>.dds. A capture that throws is
// logged and skipped.
void capture_reflection(int eye) noexcept {
    guarded("reflection capture", [&] {
        auto& ctx = context();
        if (!game::stereo::renderer(ctx.addresses)) return;
        platform::d3d11::dump_bound_target(
            game::stereo::renderer_get<ID3D11DeviceContext>(ctx.addresses, game::AiAddr::RendererContext),
            eye, ctx.log);
    });
}

} // namespace

bool install_reflections(const game::stereo::ReflectionTargets& targets) noexcept {
    auto& ctx        = context();
    const bool ready = game::stereo::gate_reflection_clear(targets, ctx.log);
    ctx.reflections.set_ready(ready);
    return ready;
}

void __fastcall render_reflection(void* pov, void*) noexcept {
    auto& ctx               = context();
    StereoInterface* dev    = game::stereo::load_device(ctx.manager);
    const DeviceState state = dev ? game::stereo::load_state(*dev) : DeviceState::None;
    ctx.reflections.note_pov({.pov    = pov,
                              .flags  = game::stereo::pov_flags(pov),
                              .state  = dev ? static_cast<int>(state) : -1,
                              .width  = game::stereo::pov_target_width(pov),
                              .height = game::stereo::pov_target_height(pov)});
    if (!ctx.reflections.per_eye(dev && state == DeviceState::Running, game::stereo::pov_renders(pov))) {
        game::stereo::render_reflection_flat(ctx.targets.render_fixes.reflection,
                                             pov); // flat, or nothing to render: as the game does
        return;
    }
    const bool capture = ctx.reflections.take_capture_request();
    game::stereo::render_reflection_per_eye(ctx.targets.render_fixes.reflection, ctx.manager, pov,
                                            capture ? &capture_reflection : nullptr);
    ctx.reflections.note_rendered();
}

void __cdecl stereo_final_draw() noexcept {
    auto& ctx           = context();
    const auto& targets = ctx.targets.render_fixes.aa;
    void* postfx        = ctx.stereo.running() ? game::stereo::postfx_instance(targets) : nullptr;
    const AaMode mode   = postfx ? game::stereo::aa_mode(postfx) : AaMode::Off;
    const bool run      = ctx.stereo_aa.runs_aa_pass(mode);
    if (postfx) ctx.stereo_aa.log_decision(mode, run);
    if (!run) {
        game::stereo::call_fullscreen_draw(targets); // as the game does
        return;
    }
    // The AA pass draws the bound effect (postprocess_stereo_final, fade
    // already in c36) into its intermediate, then filters into the output.
    game::stereo::run_aa_pass_full_screen(targets, postfx);
}

void __cdecl run_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept {
    // The runner's temporal flag, off: spatial SMAA (08 §3.2).
    const std::uint8_t spatial = 0;
    auto& ctx                  = context();
    if (StereoAa::spatial_smaa(*temporal != 0, ctx.stereo.running())) temporal = &spatial;
    game::stereo::call_smaa(ctx.targets.render_fixes.aa, color, second, temporal);
}

// Job threads; StereoDevice::running() is atomic.
bool __cdecl cull_distance(const void* bounds, const float* camera, float scale) noexcept {
    const auto& ctx = context();
    return core::stereo::cull_test_passes_in_stereo(ctx.stereo.running()) ||
           game::stereo::call_cull_distance(ctx.targets.render_fixes.cull, bounds, camera, scale);
}

// Only reached while the main POV culls: `buffer` is the context's
// occlusion buffer, which no other POV has (see core/stereo/cull_policy.h).
bool __cdecl cull_occlusion(const void* bounds, const void* ctx, const void* buffer) noexcept {
    const auto& c = context();
    return core::stereo::cull_test_passes_in_stereo(c.stereo.running()) ||
           game::stereo::call_cull_occlusion(c.targets.render_fixes.cull, bounds, ctx, buffer);
}

float __stdcall head_clip_fade(const float* eye, const float* pivot) noexcept {
    auto& ctx        = context();
    const float fade = game::stereo::call_head_clip_fade(ctx.targets.render_fixes.head_clip, eye, pivot);
    return ctx.head_clip.apply(fade, ctx.stereo.running());
}

} // namespace omvr::loader::hooks
