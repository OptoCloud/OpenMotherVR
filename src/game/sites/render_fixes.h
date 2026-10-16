// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "game/event_tap.h"
#include "game/game_image.h"
#include "game/patcher.h"
#include "game/stereo/alpha_light.h"
#include "game/stereo/cull.h"
#include "game/stereo/head_clip.h"
#include "game/stereo/mode_stack.h"
#include "game/stereo/postfx.h"
#include "game/stereo/pov.h"

#include <string_view>

namespace omvr::game::sites {

// The render fixes' areas, as their outcome lines name them.
inline constexpr std::string_view kReflectionsArea = "per-eye reflections";
inline constexpr std::string_view kStereoAaArea    = "stereo anti-aliasing";
inline constexpr std::string_view kCullingArea     = "culling";
inline constexpr std::string_view kHeadClipArea    = "head-in-wall delay";
inline constexpr std::string_view kEventTapArea    = "event tap";
inline constexpr std::string_view kAlphaLightArea  = "particle lighting";

// One outcome line per feature area at attach, so a log shows which parts of
// the mod are in place and, for those that are not, why (11).
void area_on(core::diag::ports::Log& log, std::string_view area, int sites) noexcept;
void area_off(core::diag::ports::Log& log, std::string_view area, std::string_view why) noexcept;

// 8: the game's mode-stack append, so the stereo update can raise the stack
// count through the game's allocator (05 §4.4). Nothing is written to the
// game; on a build without the address, or other code at its call, the
// update keeps to the storage the stack already has.
// The append found goes to `out`.
void find_mode_stack_append(const GameImage& g, core::diag::ports::Log& log,
                            stereo::ModeStackTargets& out) noexcept;

// The AI.exe functions the render fixes' replacements call, as the
// installer finds them. Each is filled before its call is redirected.
struct RenderFixTargets {
    stereo::ReflectionTargets reflection{};
    stereo::StereoAaTargets aa{};
    stereo::CullTargets cull{};
    stereo::HeadClipTargets head_clip{};
    stereo::AlphaLightTargets alpha_light{};
};

// The replacements of the render fixes (07, 08, dumps/13).
struct RenderFixHooks {
    const void* render_reflection;
    const void* stereo_final_draw;
    const void* run_smaa;
    const void* cull_distance;
    const void* cull_occlusion;
    const void* head_clip_fade;
    TapFn event_tap;
    TapFn* event_tap_trampoline;
    const void* alpha_light_eye_ndc;
    // Where the targets the replacements call are kept.
    RenderFixTargets* targets;
    // Gates the game's clear for the reflection targets; false if the
    // clear does not hold the expected prologue.
    bool (*install_reflections)(const stereo::ReflectionTargets& targets) noexcept;
};

// 8b: each fix checks its own sites, right before it is written, and stays
// off on its own (01 §2.5.2). A fix whose addresses are unknown for this
// build (0) is reported off for that reason.
void install_render_fixes(const GameImage& g, Patcher& patcher, const RenderFixHooks& hooks) noexcept;

} // namespace omvr::game::sites
