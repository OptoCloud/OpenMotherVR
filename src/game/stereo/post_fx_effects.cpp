// OpenMotherVR-Provenance: clean
#include "game/stereo/post_fx_effects.h"

#include "game/code_address.h"
#include "game/field.h"

namespace omvr::game::stereo {

namespace {

using LensFlareDrawFn = void(__fastcall*)(void* flares, void* edx, std::int32_t pass);
using PostFxReaderFn  = void(__fastcall*)(void* postfx, void* edx);

// Runs `reader` on `postfx` with `written` in the field at `offset`, then
// puts the game's value back, unless the game's settings handler stored a
// new one during the call: that one is kept, so the override never outlives
// the call it was made for.
template <class T>
void run_with(void* reader, void* postfx, std::ptrdiff_t offset, T written) noexcept {
    const PostFxReaderFn fn = CodeAddress{reader};
    T& value                = field<T>(postfx, offset);
    const T saved           = value;
    value                   = written;
    fn(postfx, nullptr);
    if (value == written) value = saved;
}

} // namespace

void call_lens_flare_draw(const PostFxEffectTargets& targets, void* flares, std::int32_t pass) noexcept {
    const LensFlareDrawFn draw = CodeAddress{targets.lens_flare_draw};
    draw(flares, nullptr, pass);
}

void call_chromatic_aberration(const PostFxEffectTargets& targets, void* postfx, bool off) noexcept {
    if (off) {
        run_with<std::uint8_t>(targets.chromatic_aberration, postfx, kPostFxChromaticAberration, 0);
        return;
    }
    const PostFxReaderFn fn = CodeAddress{targets.chromatic_aberration};
    fn(postfx, nullptr);
}

void call_film_grain(const PostFxEffectTargets& targets, void* postfx, bool off) noexcept {
    if (off) {
        run_with<float>(targets.film_grain, postfx, kPostFxFilmGrain, 0.0f);
        return;
    }
    const PostFxReaderFn fn = CodeAddress{targets.film_grain};
    fn(postfx, nullptr);
}

} // namespace omvr::game::stereo
