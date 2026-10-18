// OpenMotherVR-Provenance: clean
#pragma once

#include "game/stereo/pov.h"

#include <cstdint>

// The per-eye reflections' replacements beyond the render call (07 §4.7,
// §4.8): the reflection update, the renderer's viewport setter and its
// vertex constant writer, and the marks around each per-eye pass.
namespace omvr::loader::hooks {

// 07 §4.7: replaces the call of ReflectionPOV update. __thiscall, no args,
// plain ret. Game thread.
void __fastcall reflection_update(void* pov, void* edx) noexcept;

// 07 §4.8: detour of the viewport setter. __cdecl. During a per-eye pass,
// on the render thread, it keeps the viewport inside the pass's half; every
// other call passes through.
void __cdecl reflection_set_viewport(void* context, const game::stereo::GameViewport* viewport) noexcept;

// 07 §4.8: detour of the vertex constant writer. __cdecl. During a per-eye
// pass, on the render thread, a write that reaches the screen-mapping
// constant is followed by its value for the confined viewport; every other
// call passes through.
void __cdecl reflection_write_vs_constants(void* renderer, std::int32_t buffer, std::int32_t first_register,
                                           const float* data, std::int32_t register_count) noexcept;

// Around each per-eye pass (game::stereo::EyePassCallbacks): the pass for
// half `eye` runs on this thread. Render thread.
void begin_reflection_pass(int eye) noexcept;
void end_reflection_pass(int eye) noexcept;

// The replacements above, for game::stereo::detour_reflection_halves and
// redirect_reflection_update.
game::stereo::ReflectionReplacements reflection_replacements() noexcept;

} // namespace omvr::loader::hooks
