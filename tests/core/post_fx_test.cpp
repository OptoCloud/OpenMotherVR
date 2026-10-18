// OpenMotherVR-Provenance: clean
// core/render/post_fx: the 2D-only effect toggles in stereo (spec 24 §8)
// and their keys (24 §10). Expected values come from spec 24 §8, §10 and
// §11.3.

#include "core/config/settings_json.h"
#include "core/render/post_fx.h"

#include <doctest/doctest.h>

namespace {

using namespace omvr::core::render;

constexpr PostFxKeys kAllOff{.lens_flares = false, .chromatic_aberration = false, .film_grain = false};

TEST_CASE("2D-only effects: nothing is turned off in flat play (24 §8)") {
    CHECK(effects_off(PostFxKeys{}, false) == PostFxOff{});
    CHECK(effects_off(kAllOff, false) == PostFxOff{});
}

TEST_CASE("2D-only effects: in stereo each false key turns its effect off on its own (24 §8)") {
    CHECK(effects_off(PostFxKeys{}, true) == PostFxOff{});
    CHECK(effects_off(kAllOff, true) ==
          PostFxOff{.lens_flares = true, .chromatic_aberration = true, .film_grain = true});
    const PostFxOff grain = effects_off(PostFxKeys{.film_grain = false}, true);
    CHECK(grain == PostFxOff{.film_grain = true});
    const PostFxOff flares = effects_off(PostFxKeys{.lens_flares = false}, true);
    CHECK(flares == PostFxOff{.lens_flares = true});
    const PostFxOff fringes = effects_off(PostFxKeys{.chromatic_aberration = false}, true);
    CHECK(fringes == PostFxOff{.chromatic_aberration = true});
}

TEST_CASE("2D-only effects: the keys reach the render thread's view at once (24 §10)") {
    PostFxEffects fx;
    CHECK(fx.keys() == PostFxKeys{});
    CHECK(fx.off(true) == PostFxOff{});
    fx.set_keys(PostFxKeys{.chromatic_aberration = false});
    CHECK(fx.off(true) == PostFxOff{.chromatic_aberration = true});
    CHECK(fx.off(false) == PostFxOff{});
    fx.set_keys(PostFxKeys{});
    CHECK(fx.off(true) == PostFxOff{});
}

TEST_CASE("2D-only effects: the state is logged when stereo starts and when a key changes (24 §11.3)") {
    PostFxEffects fx;
    fx.set_keys(PostFxKeys{.lens_flares = false});
    // Flat: nothing to log.
    CHECK_FALSE(fx.report(false).has_value());
    // Stereo starts: once.
    const auto first = fx.report(true);
    REQUIRE(first.has_value());
    CHECK(*first == PostFxKeys{.lens_flares = false});
    CHECK_FALSE(fx.report(true).has_value());
    CHECK_FALSE(fx.report(true).has_value());
    // A key changes while stereo runs: once.
    fx.set_keys(kAllOff);
    const auto changed = fx.report(true);
    REQUIRE(changed.has_value());
    CHECK(*changed == kAllOff);
    CHECK_FALSE(fx.report(true).has_value());
    // Stereo stops, a key changes, stereo starts again: logged at the start.
    CHECK_FALSE(fx.report(false).has_value());
    fx.set_keys(PostFxKeys{});
    CHECK_FALSE(fx.report(false).has_value());
    const auto again = fx.report(true);
    REQUIRE(again.has_value());
    CHECK(*again == PostFxKeys{});
    // Stopped and started with the same keys: logged again at the start.
    CHECK_FALSE(fx.report(false).has_value());
    CHECK(fx.report(true).has_value());
}

TEST_CASE("2D-only effect keys: defaults and the file (24 §10)") {
    using namespace omvr::core::config;
    const Settings defaults{};
    CHECK(defaults.graphics.lens_flares);
    CHECK(defaults.graphics.chromatic_aberration);
    CHECK(defaults.graphics.film_grain);

    nlohmann::json root          = nlohmann::json::object();
    root["Settings"]["Graphics"] = {{"LensFlares", false}, {"ChromaticAberration", 0}, {"FilmGrain", false}};
    const Settings read          = read_config(root);
    CHECK_FALSE(read.graphics.lens_flares);
    CHECK(read.graphics.chromatic_aberration); // non-bool: default
    CHECK_FALSE(read.graphics.film_grain);

    nlohmann::json written = nlohmann::json::object();
    write_config(written, read);
    CHECK(written["Settings"]["Graphics"]["LensFlares"] == false);
    CHECK(written["Settings"]["Graphics"]["ChromaticAberration"] == true);
    CHECK(written["Settings"]["Graphics"]["FilmGrain"] == false);
}

} // namespace
