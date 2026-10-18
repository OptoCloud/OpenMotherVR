// OpenMotherVR-Provenance: clean
#include "loader/hooks/post_fx_hooks.h"

#include "core/render/post_fx.h"
#include "game/code_address.h"
#include "game/sites/post_fx_effects.h"
#include "game/stereo/post_fx_effects.h"
#include "loader/context.h"

#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

// The game's functions behind the redirected calls.
using LensFlareDrawFn = void(__fastcall*)(void* flares, void* edx, std::int32_t pass);
using PostFxReaderFn  = void(__fastcall*)(void* postfx, void* edx);
static_assert(std::is_convertible_v<decltype(&post_fx_lens_flare_draw), LensFlareDrawFn>);
static_assert(std::is_convertible_v<decltype(&post_fx_chromatic_aberration), PostFxReaderFn>);
static_assert(std::is_convertible_v<decltype(&post_fx_film_grain), PostFxReaderFn>);

constexpr std::string_view kStateLog =
    "[render] 2D effects in VR: lens flares {}, chromatic aberration {}, film grain {}";

constexpr const char* shown(bool key) noexcept { return key ? "game's" : "off"; }

// The keys' state when stereo starts and when a key changes while it runs
// (24 §11.3); nothing on the other frames.
core::render::PostFxOff this_frame(Context& ctx) noexcept {
    const bool stereo = ctx.stereo.running();
    if (const auto keys = ctx.post_fx.report(stereo)) {
        core::diag::ports::logf(ctx.log, kStateLog, shown(keys->lens_flares),
                                shown(keys->chromatic_aberration), shown(keys->film_grain));
    }
    return ctx.post_fx.off(stereo);
}

} // namespace

// While stereo runs with Graphics.LensFlares false, no flare is drawn.
void __fastcall post_fx_lens_flare_draw(void* flares, void*, std::int32_t pass) noexcept {
    auto& ctx = context();
    if (this_frame(ctx).lens_flares) return;
    game::stereo::call_lens_flare_draw(ctx.post_fx_targets, flares, pass);
}

// While stereo runs with Graphics.ChromaticAberration false, the reader sets
// the parameter as with the setting off, whatever the player's setting.
void __fastcall post_fx_chromatic_aberration(void* postfx, void*) noexcept {
    auto& ctx = context();
    game::stereo::call_chromatic_aberration(ctx.post_fx_targets, postfx,
                                            this_frame(ctx).chromatic_aberration);
}

// While stereo runs with Graphics.FilmGrain false, the grain is drawn as
// with intensity 0, whatever the player's setting.
void __fastcall post_fx_film_grain(void* postfx, void*) noexcept {
    auto& ctx = context();
    game::stereo::call_film_grain(ctx.post_fx_targets, postfx, this_frame(ctx).film_grain);
}

void install_post_fx_effects(const game::GameImage& g, game::Patcher& patcher) noexcept {
    auto& ctx                                      = context();
    const core::config::GraphicsSettings& graphics = ctx.settings.current().graphics;
    const core::render::PostFxKeys keys{.lens_flares          = graphics.lens_flares,
                                        .chromatic_aberration = graphics.chromatic_aberration,
                                        .film_grain           = graphics.film_grain};
    ctx.post_fx.set_keys(keys);
    const game::sites::PostFxEffectHooks replacements{
        .lens_flare_draw      = game::code_pointer(&post_fx_lens_flare_draw),
        .chromatic_aberration = game::code_pointer(&post_fx_chromatic_aberration),
        .film_grain           = game::code_pointer(&post_fx_film_grain),
        .targets              = &ctx.post_fx_targets,
    };
    game::sites::install_post_fx_effects(g, patcher, replacements, keys);
}

} // namespace omvr::loader::hooks
