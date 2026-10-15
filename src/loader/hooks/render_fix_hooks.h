// OpenMotherVR-Provenance: clean
#pragma once

#include "game/stereo/pov.h"

#include <cstdint>

// The replacements of the render fixes (07, 08): per-eye planar
// reflections, anti-aliasing in stereo, object culling and the head-in-wall
// blackout delay. Each keeps the ABI of the call it replaces, reads the game
// into values, asks the stereo logic and calls the game's own function.
namespace omvr::loader::hooks {

// Takes the reflection targets, gates the game's clear and records whether
// that worked; false if the clear does not hold the expected prologue (07
// §5). For game::sites::RenderFixHooks::install_reflections.
bool install_reflections(const game::stereo::ReflectionTargets& targets) noexcept;

// Replaces the call of ReflectionPOV render. __thiscall, no args. Render
// thread.
void __fastcall render_reflection(void* pov, void* edx) noexcept;

// Replaces the full-screen draw of the stereo output path. __cdecl, no args.
// Render thread.
void __cdecl stereo_final_draw() noexcept;

// Replaces the AA pass's call of the SMAA runner. __cdecl. Render thread.
void __cdecl run_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept;

// Replace two calls in the visibility loop. __cdecl, result in al. Called
// from the game's job threads: one atomic load, no virtual call.
bool __cdecl cull_distance(const void* bounds, const float* camera, float scale) noexcept;
bool __cdecl cull_occlusion(const void* bounds, const void* ctx, const void* buffer) noexcept;

// float HeadClipFade(const vec3* eye, const vec3* pivot). __stdcall, ret 8,
// result in ST0. Game thread (camera update).
float __stdcall head_clip_fade(const float* eye, const float* pivot) noexcept;

} // namespace omvr::loader::hooks
