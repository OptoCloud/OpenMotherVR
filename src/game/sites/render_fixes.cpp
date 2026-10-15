// The fixes the original lacks (FUN_1001d6f0 step 8b): planar reflections,
// anti-aliasing, culling, the head-in-wall fade and the event tap.

#include "game/sites/render_fixes.h"

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "game/addresses.h"
#include "game/patcher.h"
#include "game/stereo/cull.h"
#include "game/stereo/head_clip.h"
#include "game/stereo/mode_stack.h"
#include "game/stereo/postfx.h"

#include <cstdint>
#include <cstring>

namespace omvr::game::sites {

namespace {

namespace diag = core::diag;
using Game = GameImage;

// 8b: planar reflections render per eye.
constexpr std::string_view kReflections = "per-eye reflections";
void fix_reflections(const Game& g, const ReconRvas& r, const RenderFixHooks& hooks) noexcept {
    std::uint8_t* site = g.base + r.reflection_render_call;
    if (!calls(site, g.base + r.reflection_render)) {
        diag::logf("[hook] {}: off; ReflectionRenderCall (RVA {:08X}) is not the expected call",
                   kReflections, r.reflection_render_call);
        return;
    }
    if (!redirect_call(site, hooks.render_reflection, "reflections")) return;
    // render_reflection renders flat until the clear gate is in, so the
    // gate goes in only once the call reaches it.
    if (hooks.install_reflections({
            .render              = g.base + r.reflection_render,
            .apply_eye_transform = g.base + r.apply_eye_transform,
            .update_frustum      = g.base + r.pov_update_frustum,
            .renderer_eye_owner  = reinterpret_cast<void**>(g.base + r.renderer_eye_owner),
            .clear               = g.base + r.clear_render_target,
        })) {
        area_on(kReflections, 2);
    } else {
        diag::logf("[hook] {}: off; the clear gate at RendererClear (RVA {:08X}) does not hold the expected "
                   "prologue, reflections stay mono", kReflections, r.clear_render_target);
    }
}

// 8b: anti-aliasing in stereo. Both sites or neither: without the SMAA one,
// T2x would run temporally on the side-by-side image.
constexpr std::string_view kStereoAa = "stereo anti-aliasing";
void fix_anti_aliasing(const Game& g, const ReconRvas& r, const RenderFixHooks& hooks) noexcept {
    std::uint8_t* draw_site = g.base + r.stereo_output_draw_call;
    std::uint8_t* smaa_site = g.base + r.smaa_call;
    const std::uint8_t* getter = g.base + r.postfx_instance;  // mov eax, imm32; ret
    constexpr std::uint8_t kAaPrologue[6] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0};
    if (!calls(draw_site, g.base + r.fullscreen_draw) || !calls(smaa_site, g.base + r.smaa) ||
        getter[0] != 0xB8 || getter[5] != 0xC3 ||
        std::memcmp(g.base + r.aa_pass, kAaPrologue, sizeof(kAaPrologue)) != 0) {
        diag::logf("[hook] {}: off; unexpected code at StereoOutputDrawCall (RVA {:08X}), SmaaCall (RVA {:08X}), "
                   "PostFxInstance or AaPass", kStereoAa, r.stereo_output_draw_call, r.smaa_call);
        return;
    }
    stereo::install_stereo_aa({
        .fullscreen_draw = g.base + r.fullscreen_draw,
        .postfx          = g.base + r.postfx_instance,
        .aa_pass         = g.base + r.aa_pass,
        .smaa            = g.base + r.smaa,
    });
    if (redirect_call(smaa_site, hooks.run_smaa, "SMAA") &&
        redirect_call(draw_site, hooks.stereo_final_draw, "stereo output draw")) {
        area_on(kStereoAa, 2);
    }
}

// 8b: object culling. In stereo the distance/LOD test passes in every POV
// group and the main POV's centre-view occlusion test passes; the frustum
// and the shadow lights' view-volume test stay the game's.
constexpr std::string_view kCulling = "culling";
void fix_culling(const Game& g, const ReconRvas& r, const RenderFixHooks& hooks) noexcept {
    // mov eax,[esi+0x5C]; test eax,eax; je +0xF; push eax; push esi; push ebx:
    // the occlusion test takes the context's occlusion buffer, which only
    // the main POV has, so the replacement needs no phase check.
    constexpr std::uint8_t kOcclusionArgs[] = {0x8B, 0x46, 0x5C, 0x85, 0xC0, 0x74, 0x0F, 0x50, 0x56, 0x53};
    const std::uint8_t* occlusion_args = g.base + r.cull_occlusion_call - sizeof(kOcclusionArgs);
    if (!calls(g.base + r.cull_distance_call, g.base + r.cull_distance) ||
        !calls(g.base + r.cull_occlusion_call, g.base + r.cull_occlusion) ||
        std::memcmp(occlusion_args, kOcclusionArgs, sizeof(kOcclusionArgs)) != 0) {
        diag::logf("[hook] {}: off; unexpected code at CullDistanceCall (RVA {:08X}) or CullPlaneTestACall "
                   "(RVA {:08X})", kCulling, r.cull_distance_call, r.cull_occlusion_call);
        return;
    }
    stereo::install_cull_fix({
        .distance  = g.base + r.cull_distance,
        .occlusion = g.base + r.cull_occlusion,
    });
    if (redirect_call(g.base + r.cull_distance_call, hooks.cull_distance, "distance cull") &&
        redirect_call(g.base + r.cull_occlusion_call, hooks.cull_occlusion, "occlusion cull")) {
        area_on(kCulling, 2);
        diag::log_line("[hook] culling: in stereo the distance test passes and the main view's occlusion test "
                       "passes");
    }
}

// 8b: head-in-wall blackout. In stereo a fade waits 0.6 s before it shows.
constexpr std::string_view kHeadClip = "head-in-wall delay";
void fix_head_clip_fade(const Game& g, const ReconRvas& r, const RenderFixHooks& hooks) noexcept {
    if (!calls(g.base + r.head_clip_fade_call, g.base + r.head_clip_fade)) {
        diag::logf("[hook] {}: off; HeadClipFadeCall (RVA {:08X}) is not the expected call", kHeadClip,
                   r.head_clip_fade_call);
        return;
    }
    stereo::install_head_clip_delay(g.base + r.head_clip_fade);
    if (redirect_call(g.base + r.head_clip_fade_call, hooks.head_clip_fade, "HeadClipFade")) {
        area_on(kHeadClip, 1);
    }
}

// 8b: interaction and hiding state from the scripts' event strings, for the
// features that behave differently during interactions.
constexpr std::string_view kEventTap = "event tap";
void tap_game_events(const Game& g, const ReconRvas& r, const RenderFixHooks& hooks) noexcept {
    if (install_event_tap(g.base + r.event_tap, hooks.event_tap, *hooks.event_tap_original)) {
        area_on(kEventTap, 1);
    } else {
        diag::logf("[hook] {}: off; ScriptEventTap (RVA {:08X}) does not hold the expected no-op or could not "
                   "be detoured", kEventTap, r.event_tap);
    }
}

} // namespace

void area_on(std::string_view area, int sites) noexcept {
    diag::logf("[hook] {}: in place on {} sites", area, sites);
}

void area_off(std::string_view area, std::string_view why) noexcept {
    diag::logf("[hook] {}: off; {}", area, why);
}

// 8: the game's mode-stack append, so the stereo update can raise the stack
// count through the game's allocator. Nothing is written to the game; on a
// build without the address, or other code at its call, the update keeps to
// the storage the stack already has.
void find_mode_stack_append(const Game& g) noexcept {
    const ReconRvas& r = recon_rvas(g.version);
    if (r.mode_stack_append == 0) {
        diag::log_line("[hook] mode stack: this build has no address for the game's append; the count is raised "
                       "only into existing storage");
        return;
    }
    constexpr std::uint8_t kLeaEcxStack[] = {0x8D, 0x4B, 0x68};   // lea ecx, [ebx+0x68]
    const std::uint8_t* site = g.base + r.mode_stack_append_call;
    if (std::memcmp(site - sizeof(kLeaEcxStack), kLeaEcxStack, sizeof(kLeaEcxStack)) != 0 ||
        !calls(site, g.base + r.mode_stack_append)) {
        diag::logf("[hook] mode stack: StereoModeStackAppend's call (RVA {:08X}) is not the expected code; the "
                   "count is raised only into existing storage", r.mode_stack_append_call);
        return;
    }
    stereo::install_mode_stack_append(g.base + r.mode_stack_append);
    diag::logf("[hook] mode stack: count raised through the game's append (RVA {:08X})", r.mode_stack_append);
}

void install_reconstruction_fixes(const Game& g, const RenderFixHooks& hooks) noexcept {
    constexpr std::string_view kNoAddress = "this build has no address for it";
    const ReconRvas& r = recon_rvas(g.version);
    if (r.reflection_render_call != 0) fix_reflections(g, r, hooks); else area_off(kReflections, kNoAddress);
    if (r.stereo_output_draw_call != 0) fix_anti_aliasing(g, r, hooks); else area_off(kStereoAa, kNoAddress);
    if (r.cull_distance_call != 0) fix_culling(g, r, hooks); else area_off(kCulling, kNoAddress);
    if (r.head_clip_fade_call != 0) fix_head_clip_fade(g, r, hooks); else area_off(kHeadClip, kNoAddress);
    if (r.event_tap != 0) tap_game_events(g, r, hooks); else area_off(kEventTap, kNoAddress);
}

} // namespace omvr::game::sites
