#pragma once

#include <cstdint>

// The replacements of the render fixes the original lacks: per-eye planar
// reflections, anti-aliasing in stereo, object culling and the head-in-wall
// blackout delay. Each keeps the ABI of the call it replaces.
namespace omvr::loader::hooks {

// Replaces the call of ReflectionPOV render. __thiscall, no args.
void __fastcall render_reflection(void* pov, void* edx) noexcept;

// Replaces the full-screen draw of the stereo output path. __cdecl, no args.
void __cdecl stereo_final_draw() noexcept;

// Replaces the AA pass's call of the SMAA runner. __cdecl.
void __cdecl run_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept;

// Replace two calls in the visibility loop. __cdecl, result in al. Called
// from the game's job threads.
bool __cdecl cull_distance(const void* bounds, const float* camera, float scale) noexcept;
bool __cdecl cull_occlusion(const void* bounds, const void* ctx, const void* buffer) noexcept;

// float HeadClipFade(const vec3* eye, const vec3* pivot). __stdcall, ret 8,
// result in ST0.
float __stdcall head_clip_fade(const float* eye, const float* pivot) noexcept;

} // namespace omvr::loader::hooks
