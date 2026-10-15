#pragma once

#include "game/constants.h"

#include <cstddef>
#include <cstdint>

// The game's post-processing (PostFX): its anti-aliasing pass and the
// stereo output draw (08 §3).
namespace omvr::game::stereo {

// PostFX fields (08 §3.1).
inline constexpr std::ptrdiff_t kPostFxMeshDraw = 0x1B2F; // the AA pass draws with its own mesh instead
                                                          // of the full-screen triangle
inline constexpr std::ptrdiff_t kPostFxAaMode = 0x1B3C;   // AaMode, setting 9

// The AI.exe functions the stereo AA fix calls, found by the installer.
struct StereoAaTargets {
    void* fullscreen_draw; // full-screen triangle draw (cdecl, no args)
    void* postfx;          // PostFX instance getter (cdecl, no args)
    void* aa_pass;         // PostFX AA pass (thiscall, no args)
    void* smaa;            // SMAA runner (cdecl; color, second input, const bool* temporal)
};

// The calls below go through `targets`, set once at attach by the installer
// before the calls are redirected.

// The full-screen triangle draw. Render thread, as all below.
void call_fullscreen_draw(const StereoAaTargets& targets) noexcept;
// The PostFX instance.
void* postfx_instance(const StereoAaTargets& targets) noexcept;
// The anti-aliasing mode of `postfx`.
AaMode aa_mode(const void* postfx) noexcept;
// The AA pass on `postfx`, drawing its input with the full-screen triangle
// as the stereo path always does (the mesh flag is cleared for the call and
// put back after it).
void run_aa_pass_full_screen(const StereoAaTargets& targets, void* postfx) noexcept;
// The SMAA runner.
void call_smaa(const StereoAaTargets& targets, void* color, void* second,
               const std::uint8_t* temporal) noexcept;

} // namespace omvr::game::stereo
