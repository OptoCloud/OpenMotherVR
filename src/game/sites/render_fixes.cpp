// OpenMotherVR-Provenance: clean
// The render fixes (07, 08): planar reflections, anti-aliasing, culling, the
// head-in-wall fade, the event tap and the particle lighting lookup
// (dumps/13). Each checks its own code right before it is written and stays
// off on its own (01 §2.5.2).

#include "game/sites/render_fixes.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/sites/area_log.h"
#include "game/stereo/cull.h"
#include "game/stereo/head_clip.h"
#include "game/stereo/postfx.h"
#include "game/x86.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace omvr::game::sites {

namespace {

namespace ports = core::diag::ports;

// ---- Areas, their site counts and log templates -------------------------------

// The render call, the clear gate, the viewport setter and the vertex
// constant writer; the update call when it goes in too.
constexpr int kReflectionSites = 4;
constexpr int kHeadClipSites   = 1;
constexpr int kEventTapSites   = 1;
constexpr int kAlphaLightSites = 1;

constexpr std::string_view kReflectionCallLog =
    "[hook] {}: off; ReflectionRenderCall (RVA {:08X}) is not the expected call";
constexpr std::string_view kClearGateLog =
    "[hook] {}: off; the clear gate at RendererClear (RVA {:08X}) does not hold the expected prologue, "
    "reflections stay mono";
constexpr std::string_view kHalvesLog =
    "[hook] {}: off; ViewportSetter (RVA {:08X}) or VsConstantWriter (RVA {:08X}) does not hold the expected "
    "prologue or could not be detoured, reflections stay mono";
// 07 §6: whether the object list covers both eyes (07 §4.7), once.
constexpr std::string_view kBothEyesLog =
    "[hook] {}: the reflection's object list covers both eyes (ReflectionUpdateCall, RVA {:08X})";
constexpr std::string_view kNotBothEyesLog =
    "[hook] {}: on without the both-eye object list; ReflectionUpdateCall (RVA {:08X}) is not the expected "
    "call, objects at the outer edges of each eye's reflection may be missing";
constexpr std::string_view kStereoAaCodeLog =
    "[hook] {}: off; unexpected code at StereoOutputDrawCall (RVA {:08X}), SmaaCall (RVA {:08X}), "
    "PostFxInstance or AaPass";
constexpr std::string_view kCullingCodeLog =
    "[hook] {}: off; unexpected code at CullDistanceCall (RVA {:08X}) or CullPlaneTestACall (RVA {:08X})";
constexpr std::string_view kCullingOnLog =
    "[hook] culling: in stereo the distance test passes and the main view's occlusion test passes";
constexpr std::string_view kHeadClipCallLog =
    "[hook] {}: off; HeadClipFadeCall (RVA {:08X}) is not the expected call";
constexpr std::string_view kEventTapLog =
    "[hook] {}: off; ScriptEventTap (RVA {:08X}) does not hold the expected no-op or could not be detoured";
constexpr std::string_view kAlphaLightCallLog =
    "[hook] {}: off; AlphaLightNdcCall (RVA {:08X}) is not the expected call";

// ---- The code the fixes expect --------------------------------------------------

// 08 §3: the PostFX getter is `mov eax, imm32; ret`.
constexpr std::size_t kGetterRetOffset = x86::kMovEaxImm32Size;

// 08 §3: the AA pass's prologue, `push ebp; mov ebp, esp; and esp, -16`.
constexpr auto kAaPassPrologue = x86::kAlignedFramePrologue;

// 08 §2: before the occlusion test's call, `mov eax, [esi+0x5C]; test eax,
// eax; je +0xF; push eax; push esi; push ebx`: the test takes the context's
// occlusion buffer, which only the main POV has, so the replacement needs
// no phase check.
constexpr std::uint8_t kOcclusionBufferDisp           = 0x5C; // [esi+0x5C]
constexpr std::uint8_t kNoBufferSkip                  = 0x0F; // je over the call
constexpr std::array<std::uint8_t, 10> kOcclusionArgs = {
    x86::kMovR32Rm32,     x86::modrm_disp8(x86::Reg::Eax, x86::Reg::Esi),
    kOcclusionBufferDisp, x86::kTestRm32R32,
    x86::kModRmEaxEax,    x86::kJeRel8,
    kNoBufferSkip,        x86::kPushEax,
    x86::kPushEsi,        x86::kPushEbx};
static_assert(kOcclusionArgs ==
              std::array<std::uint8_t, 10>{0x8B, 0x46, 0x5C, 0x85, 0xC0, 0x74, 0x0F, 0x50, 0x56, 0x53});

bool postfx_getter_is_expected(const std::uint8_t* getter) noexcept {
    return getter[0] == x86::kMovEaxImm32 && getter[kGetterRetOffset] == x86::kRet;
}

// ---- One function per fix -------------------------------------------------------

// Logs what install_reflections put in (07 §5, §6).
void log_reflections(ports::Log& log, const ReconRvas& r, const stereo::ReflectionInstall& in) noexcept {
    if (!in.clear_gated) {
        ports::logf(log, kClearGateLog, kReflectionsArea, r.clear_render_target);
        return;
    }
    if (!in.halves) {
        ports::logf(log, kHalvesLog, kReflectionsArea, r.viewport_setter, r.vs_constant_writer);
        return;
    }
    area_on(log, kReflectionsArea, kReflectionSites + (in.covers_both_eyes ? 1 : 0));
    if (in.covers_both_eyes) {
        ports::logf(log, kBothEyesLog, kReflectionsArea, r.reflection_update_call);
    } else {
        ports::logf(log, kNotBothEyesLog, kReflectionsArea, r.reflection_update_call);
    }
}

// Planar reflections render per eye (07). Every site is checked before the
// first write (07 §5); only the update call may differ, and the fix then
// goes in without the both-eye object list (07 §4.7).
void fix_reflections(const GameImage& g, Patcher& p, const ReconRvas& r,
                     const RenderFixHooks& hooks) noexcept {
    ports::Log& log    = p.log();
    std::uint8_t* site = g.at_rva(r.reflection_render_call);
    if (!calls(site, g.at_rva(r.reflection_render))) {
        ports::logf(log, kReflectionCallLog, kReflectionsArea, r.reflection_render_call);
        return;
    }
    // A pass that drew outside its half would erase the other's (07 §5).
    if (!x86::matches(g.at_rva(r.viewport_setter), stereo::kViewportSetterPrologue) ||
        !x86::matches(g.at_rva(r.vs_constant_writer), stereo::kVsConstantWriterPrologue)) {
        ports::logf(log, kHalvesLog, kReflectionsArea, r.viewport_setter, r.vs_constant_writer);
        return;
    }
    std::uint8_t* update_call = g.at_rva(r.reflection_update_call);
    const bool update_ok      = calls(update_call, g.at_rva(r.reflection_update));
    hooks.targets->reflection = {
        .render              = g.at_rva(r.reflection_render),
        .apply_eye_transform = g.at_rva(r.apply_eye_transform),
        .update_frustum      = g.at_rva(r.pov_update_frustum),
        .renderer_eye_owner  = CodeAddress{g.at_rva(r.renderer_eye_owner)},
        .clear               = g.at_rva(r.clear_render_target),
        .reflection_manager  = CodeAddress{g.at_rva(r.reflection_manager)},
        .update              = g.at_rva(r.reflection_update),
        .update_call         = update_ok ? update_call : nullptr,
        .viewport_setter     = g.at_rva(r.viewport_setter),
        .vs_constant_writer  = g.at_rva(r.vs_constant_writer),
    };
    if (!p.redirect_call(site, hooks.render_reflection, "reflections")) return;
    // render_reflection renders flat until all of it is in, so the rest
    // goes in only once the call reaches it.
    log_reflections(log, r, hooks.install_reflections(hooks.targets->reflection));
}

// Anti-aliasing in stereo (08 §3). Both sites or neither: without the SMAA
// one, T2x would run temporally on the side-by-side image.
void fix_anti_aliasing(const GameImage& g, Patcher& p, const ReconRvas& r,
                       const RenderFixHooks& hooks) noexcept {
    std::uint8_t* draw_site = g.at_rva(r.stereo_output_draw_call);
    std::uint8_t* smaa_site = g.at_rva(r.smaa_call);
    if (!calls(draw_site, g.at_rva(r.fullscreen_draw)) || !calls(smaa_site, g.at_rva(r.smaa)) ||
        !postfx_getter_is_expected(g.at_rva(r.postfx_instance)) ||
        !x86::matches(g.at_rva(r.aa_pass), kAaPassPrologue)) {
        ports::logf(p.log(), kStereoAaCodeLog, kStereoAaArea, r.stereo_output_draw_call, r.smaa_call);
        return;
    }
    hooks.targets->aa = {
        .fullscreen_draw = g.at_rva(r.fullscreen_draw),
        .postfx          = g.at_rva(r.postfx_instance),
        .aa_pass         = g.at_rva(r.aa_pass),
        .smaa            = g.at_rva(r.smaa),
    };
    // The SMAA call and the stereo output draw.
    const std::array calls{CallRedirect{smaa_site, hooks.run_smaa, "SMAA"},
                           CallRedirect{draw_site, hooks.stereo_final_draw, "stereo output draw"}};
    static_assert(calls.size() == 2);
    if (redirect_all(p, calls)) area_on(p.log(), kStereoAaArea, static_cast<int>(calls.size()));
}

// Object culling (08 §2). In stereo the distance/LOD test passes in every
// POV group and the main POV's centre-view occlusion test passes; the
// frustum and the shadow lights' view-volume test stay the game's.
void fix_culling(const GameImage& g, Patcher& p, const ReconRvas& r, const RenderFixHooks& hooks) noexcept {
    // The caller checked cull_distance_call only; the bytes before the
    // occlusion call are read only once its row is known.
    if (r.cull_occlusion_call == 0 || r.cull_distance == 0 || r.cull_occlusion == 0 ||
        !calls(g.at_rva(r.cull_distance_call), g.at_rva(r.cull_distance)) ||
        !calls(g.at_rva(r.cull_occlusion_call), g.at_rva(r.cull_occlusion)) ||
        !x86::matches(g.at_rva(r.cull_occlusion_call) - kOcclusionArgs.size(), kOcclusionArgs)) {
        ports::logf(p.log(), kCullingCodeLog, kCullingArea, r.cull_distance_call, r.cull_occlusion_call);
        return;
    }
    hooks.targets->cull = {
        .distance  = g.at_rva(r.cull_distance),
        .occlusion = g.at_rva(r.cull_occlusion),
    };
    // The distance and the occlusion test calls.
    const std::array calls{
        CallRedirect{g.at_rva(r.cull_distance_call), hooks.cull_distance, "distance cull"},
        CallRedirect{g.at_rva(r.cull_occlusion_call), hooks.cull_occlusion, "occlusion cull"}};
    static_assert(calls.size() == 2);
    if (redirect_all(p, calls)) {
        area_on(p.log(), kCullingArea, static_cast<int>(calls.size()));
        p.log().line(kCullingOnLog);
    }
}

// Head-in-wall blackout (08 §4). In stereo a fade waits 0.6 s before it
// shows.
void fix_head_clip_fade(const GameImage& g, Patcher& p, const ReconRvas& r,
                        const RenderFixHooks& hooks) noexcept {
    if (!calls(g.at_rva(r.head_clip_fade_call), g.at_rva(r.head_clip_fade))) {
        ports::logf(p.log(), kHeadClipCallLog, kHeadClipArea, r.head_clip_fade_call);
        return;
    }
    hooks.targets->head_clip = {.fade = g.at_rva(r.head_clip_fade)};
    if (p.redirect_call(g.at_rva(r.head_clip_fade_call), hooks.head_clip_fade, "HeadClipFade")) {
        area_on(p.log(), kHeadClipArea, kHeadClipSites);
    }
}

// Interaction and hiding state from the scripts' event strings, for the
// features that behave differently during interactions (05 §12).
void tap_game_events(const GameImage& g, Patcher& p, const ReconRvas& r,
                     const RenderFixHooks& hooks) noexcept {
    if (install_event_tap(g.at_rva(r.event_tap), hooks.event_tap, *hooks.event_tap_trampoline, p.log())) {
        area_on(p.log(), kEventTapArea, kEventTapSites);
    } else {
        ports::logf(p.log(), kEventTapLog, kEventTapArea, r.event_tap);
    }
}

// Particle lighting (dumps/13 §6): in stereo the atlas lookup gets the
// eye's NDC scale and offset relative to the centre camera the atlas was
// laid out with.
void fix_alpha_light(const GameImage& g, Patcher& p, const ReconRvas& r,
                     const RenderFixHooks& hooks) noexcept {
    if (!calls(g.at_rva(r.alpha_light_ndc_call), g.at_rva(r.eye_ndc))) {
        ports::logf(p.log(), kAlphaLightCallLog, kAlphaLightArea, r.alpha_light_ndc_call);
        return;
    }
    hooks.targets->alpha_light = {.eye_ndc          = g.at_rva(r.eye_ndc),
                                  .light_object     = g.at_rva(r.light_object),
                                  .camera_constants = g.at_rva(r.camera_constants)};
    if (p.redirect_call(g.at_rva(r.alpha_light_ndc_call), hooks.alpha_light_eye_ndc, "EyeNdc")) {
        area_on(p.log(), kAlphaLightArea, kAlphaLightSites);
    }
}

using Fix = void (*)(const GameImage&, Patcher&, const ReconRvas&, const RenderFixHooks&) noexcept;

// Runs `fix` when this build has its address (`rva`), else reports it off.
void fix_if_known(std::uint32_t rva, std::string_view area, Fix fix, const GameImage& g, Patcher& p,
                  const ReconRvas& r, const RenderFixHooks& hooks) noexcept {
    if (known_or_off(p.log(), rva, area)) fix(g, p, r, hooks);
}

} // namespace

void install_render_fixes(const GameImage& g, Patcher& p, const RenderFixHooks& hooks) noexcept {
    const ReconRvas& r = kReconRvas[g.version];
    fix_if_known(r.reflection_render_call, kReflectionsArea, &fix_reflections, g, p, r, hooks);
    fix_if_known(r.stereo_output_draw_call, kStereoAaArea, &fix_anti_aliasing, g, p, r, hooks);
    fix_if_known(r.cull_distance_call, kCullingArea, &fix_culling, g, p, r, hooks);
    fix_if_known(r.head_clip_fade_call, kHeadClipArea, &fix_head_clip_fade, g, p, r, hooks);
    fix_if_known(r.event_tap, kEventTapArea, &tap_game_events, g, p, r, hooks);
    fix_if_known(r.alpha_light_ndc_call, kAlphaLightArea, &fix_alpha_light, g, p, r, hooks);
}

} // namespace omvr::game::sites
