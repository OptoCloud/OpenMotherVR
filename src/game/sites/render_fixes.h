#pragma once

#include "game/event_tap.h"
#include "game/preflight.h"
#include "game/stereo/pov.h"

#include <string_view>

namespace omvr::game::sites {

// One outcome line per feature area at attach, so a log shows which parts of
// the mod are in place and, for those that are not, why.
void area_on(std::string_view area, int sites) noexcept;
void area_off(std::string_view area, std::string_view why) noexcept;

// 8: the game's mode-stack append, so the stereo update can raise the stack
// count through the game's allocator. Nothing is written to the game; on a
// build without the address, or other code at its call, the update keeps to
// the storage the stack already has.
void find_mode_stack_append(const GameImage& g) noexcept;

// The replacements of the fixes the original lacks.
struct RenderFixHooks {
    const void* render_reflection;
    const void* stereo_final_draw;
    const void* run_smaa;
    const void* cull_distance;
    const void* cull_occlusion;
    const void* head_clip_fade;
    TapFn       event_tap;
    TapFn*      event_tap_original;
    // Takes the reflection targets and gates the game's clear; false if the
    // clear does not hold the expected prologue.
    bool (*install_reflections)(const stereo::ReflectionTargets& targets) noexcept;
};

// 8b: each fix checks its own sites and stays off on its own. A fix whose
// addresses are unknown for this build (0) is reported off for that reason.
void install_reconstruction_fixes(const GameImage& g, const RenderFixHooks& hooks) noexcept;

} // namespace omvr::game::sites
