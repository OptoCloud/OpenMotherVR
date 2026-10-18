// OpenMotherVR-Provenance: clean
#pragma once

#include "core/render/types.h"
#include "game/game_image.h"
#include "game/patcher.h"
#include "game/stereo/post_fx_effects.h"

#include <string_view>

namespace omvr::game::sites {

// The toggles' areas, as their outcome lines name them (24 §11.3).
inline constexpr std::string_view kLensFlareToggleArea           = "lens flare toggle";
inline constexpr std::string_view kChromaticAberrationToggleArea = "chromatic aberration toggle";
inline constexpr std::string_view kFilmGrainToggleArea           = "film grain toggle";

// The replacements of the toggles (24 §8), each with the ABI of the call it
// takes over.
struct PostFxEffectHooks {
    const void* lens_flare_draw;      // __fastcall (flares, edx, int pass)
    const void* chromatic_aberration; // __fastcall (postfx, edx)
    const void* film_grain;           // __fastcall (postfx, edx)
    // Where the targets the replacements call are kept.
    stereo::PostFxEffectTargets* targets;
};

// Installs the toggle of each key that is false at attach (with every key
// at its default nothing is written, 24 §8 and 20 §1.1). Each checks its
// own sites right before it is written and stays off on its own, logged;
// on a build without the addresses each is reported off.
void install_post_fx_effects(const GameImage& g, Patcher& patcher, const PostFxEffectHooks& hooks,
                             const core::render::PostFxKeys& keys) noexcept;

} // namespace omvr::game::sites
