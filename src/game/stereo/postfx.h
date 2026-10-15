#pragma once

#include <cstddef>
#include <cstdint>

// The game's post-processing (PostFX): its anti-aliasing pass and the
// stereo output draw.
namespace omvr::game::stereo {

// PostFX fields.
inline constexpr std::ptrdiff_t kPostFxMeshDraw = 0x1B2F;  // AA pass draws with its own mesh instead of
                                                    // the full-screen triangle
inline constexpr std::ptrdiff_t kPostFxAaMode   = 0x1B3C;  // setting 9

enum AaMode : std::int32_t { kAaOff = 0, kFxaa = 1, kSmaaT1x = 2, kSmaaT2x = 3 };

struct StereoAaTargets {
    void* fullscreen_draw;  // full-screen triangle draw (cdecl, no args)
    void* postfx;           // PostFX instance getter (cdecl, no args)
    void* aa_pass;          // PostFX AA pass (thiscall, no args)
    void* smaa;             // SMAA runner (cdecl; color, second input, const bool* temporal)
};

// The AI.exe functions, set once at attach by the installer.
void install_stereo_aa(const StereoAaTargets& targets) noexcept;

// The full-screen triangle draw.
void call_fullscreen_draw() noexcept;
// The PostFX instance.
void* postfx_instance() noexcept;
// The AA pass on `postfx`.
void call_aa_pass(void* postfx) noexcept;
// The SMAA runner.
void call_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept;

} // namespace omvr::game::stereo
