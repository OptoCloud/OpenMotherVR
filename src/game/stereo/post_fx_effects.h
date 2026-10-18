// OpenMotherVR-Provenance: clean
#pragma once

#include <cstddef>
#include <cstdint>

// The game's 2D-only effects (spec 24 §1.6, §1.8) as the stereo toggles of
// 24 §8 reach them: the lens flare draw, and the post-processing readers of
// the chromatic aberration flag and the film grain intensity.
namespace omvr::game::stereo {

// Post-processing fields the readers use (24 §1.6), written by the game's
// settings handler from settings 0x0E and 0x0F.
inline constexpr std::ptrdiff_t kPostFxFilmGrain           = 0x608;  // float, [0, 1]
inline constexpr std::ptrdiff_t kPostFxChromaticAberration = 0x1B38; // bool

// The AI.exe functions the toggles' replacements call, found by the
// installer and set once at attach before their calls are redirected.
struct PostFxEffectTargets {
    void* lens_flare_draw;      // thiscall (lensflares, int pass), ret 4
    void* chromatic_aberration; // thiscall (postfx), no args: sets material parameter 2
    void* film_grain;           // thiscall (postfx), no args: sets material parameter 9, among others
};

// The calls below run on the render thread.

// The game's lens flare draw.
void call_lens_flare_draw(const PostFxEffectTargets& targets, void* flares, std::int32_t pass) noexcept;

// The game's chromatic aberration reader on `postfx`; with `off`, it runs
// as when the setting is off (the parameter is -1.0). The game's flag is
// put back after the call.
void call_chromatic_aberration(const PostFxEffectTargets& targets, void* postfx, bool off) noexcept;

// The game's film grain reader on `postfx`; with `off`, it runs as with
// intensity 0. The game's intensity is put back after the call.
void call_film_grain(const PostFxEffectTargets& targets, void* postfx, bool off) noexcept;

} // namespace omvr::game::stereo
