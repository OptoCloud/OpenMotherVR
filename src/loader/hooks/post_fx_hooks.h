// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"
#include "game/patcher.h"

#include <cstdint>

// The 2D-only effects in stereo (spec 24 §8): the calls of the lens flare
// draw and of the chromatic aberration and film grain readers.
namespace omvr::loader::hooks {

// Replaces the post-processing render's call of the lens flare draw.
// void __thiscall (lensflares, int pass), ret 4. Render thread.
void __fastcall post_fx_lens_flare_draw(void* flares, void* edx, std::int32_t pass) noexcept;

// Replaces the post-processing render's call of the chromatic aberration
// reader. void __thiscall (postfx), no args. Render thread.
void __fastcall post_fx_chromatic_aberration(void* postfx, void* edx) noexcept;

// Replaces the call of the film grain reader. void __thiscall (postfx), no
// args. Render thread.
void __fastcall post_fx_film_grain(void* postfx, void* edx) noexcept;

// Takes the Graphics keys of 24 §8 and installs the toggle of each that is
// false (24 §8, 20 §1.1). Main thread, at attach, in the full patch set.
void install_post_fx_effects(const game::GameImage& g, game::Patcher& patcher) noexcept;

} // namespace omvr::loader::hooks
