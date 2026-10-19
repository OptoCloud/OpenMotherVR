// OpenMotherVR-Provenance: clean
// The interaction and HUD options (spec 23 §5 to §8, 22 §8.3): their keys,
// the player's HUD state, the crosshair gate, the toggle gesture, the HUD
// panel's placement and the UI patch's variant entries. Expected values
// are the spec's.

#include "core/config/settings_json.h"
#include "core/config/settings_table.h"
#include "core/config/tuning.h"
#include "core/stereo/tuning.h"
#include "core/stereo/ui_placement.h"
#include "core/ui/commands.h"
#include "core/ui/crosshair_gate.h"
#include "core/ui/hud_placement.h"
#include "core/ui/hud_toggle.h"
#include "core/ui/hud_visibility.h"
#include "core/ui/pak2.h"
#include "core/ui/ui_patch.h"
#include "fakes/fakes.h"
#include "util/hash/crc32.h"
#include "util/math/matrix.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <string_view>
#include <vector>

namespace {

using namespace std::chrono_literals;
using nlohmann::json;
using omvr::core::config::CameraTakeovers;
using omvr::core::config::CrosshairMode;
using omvr::core::config::HudPlacement;
using omvr::core::config::read_config;
using omvr::core::config::sanitize;
using omvr::core::config::Settings;
using omvr::core::config::write_config;
using omvr::core::stereo::Matrix44;
using omvr::core::ui::CrosshairGate;
using omvr::core::ui::HudAction;
using omvr::core::ui::HudMovieStates;
using omvr::core::ui::HudPanel;
using omvr::core::ui::HudPlacer;
using omvr::core::ui::HudToggleGesture;
using omvr::core::ui::HudVisibility;
using Clock = std::chrono::steady_clock;

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kDeg = std::numbers::pi_v<float> / 180.0f;

Settings sanitized(Settings s) {
    sanitize(s);
    return s;
}

// A view looking along heading `h` (radians about +Y, 0 along +Z) from the
// origin: its columns are the camera's right, up and forward axes.
Matrix44 view_towards(float h) {
    Matrix44 m{};
    omvr::util::identity(m.data());
    m[0]  = std::cos(h);
    m[8]  = -std::sin(h);
    m[2]  = std::sin(h);
    m[10] = std::cos(h);
    return m;
}

// The heading a level panel faces, from its forward row.
float panel_heading(const Matrix44& q) { return std::atan2(q[8], q[10]); }

HudMovieStates movies(bool weapon_hidden, bool pickup_hidden) {
    return {{{true, weapon_hidden}, {true, pickup_hidden}}};
}

} // namespace

TEST_SUITE("interaction and HUD settings") {
    TEST_CASE("absent keys give v1's values") {
        const Settings s = read_config(json::parse(R"({"Settings": {}})"));
        CHECK_FALSE(s.interaction.physical_levers);
        CHECK(s.interaction.camera_takeovers == CameraTakeovers::Game);
        CHECK_FALSE(s.interaction.fast_animations);
        CHECK(s.interaction.terminal_distance == 0.0f);
        CHECK(s.interaction.rewire_distance == 0.0f);
        CHECK(s.interaction.terminal_text_scale == 1.0f);
        CHECK(s.hud.crosshair == CrosshairMode::On);
        CHECK_FALSE(s.hud.hidden);
        CHECK(s.hud.item_highlight);
        CHECK(s.hud.placement == HudPlacement::HeadLocked);
        CHECK(s.hud.distance == 1.0f);
        CHECK(s.hud.width == 0.55f);
        CHECK(s.hud.pitch == 0.0f);
    }

    TEST_CASE("the keys are read from Interaction and Hud, strings case-sensitive") {
        const Settings s = read_config(json::parse(R"({"Settings": {
            "Interaction": {"PhysicalLevers": true, "CameraTakeovers": "Reduced", "TerminalDistance": 1.0,
                            "RewireDistance": 1.2, "TerminalTextScale": 1.5},
            "Hud": {"Crosshair": "Auto", "Hidden": true, "ItemHighlight": false, "Placement": "Follow",
                    "Distance": 1.5, "Width": 0.8, "Pitch": -15}}})"));
        CHECK(s.interaction.physical_levers);
        CHECK(s.interaction.camera_takeovers == CameraTakeovers::Reduced);
        CHECK(s.interaction.terminal_distance == 1.0f);
        CHECK(s.interaction.rewire_distance == doctest::Approx(1.2f));
        CHECK(s.interaction.terminal_text_scale == 1.5f);
        CHECK(s.hud.crosshair == CrosshairMode::Auto);
        CHECK(s.hud.hidden);
        CHECK_FALSE(s.hud.item_highlight);
        CHECK(s.hud.placement == HudPlacement::Follow);
        CHECK(s.hud.pitch == -15.0f);

        const Settings lower = read_config(json::parse(
            R"({"Settings": {"Interaction": {"CameraTakeovers": "reduced"},
                             "Hud": {"Crosshair": "off", "Placement": "follow", "Hidden": "yes"}}})"));
        CHECK(lower.interaction.camera_takeovers == CameraTakeovers::Game);
        CHECK(lower.hud.crosshair == CrosshairMode::On);
        CHECK(lower.hud.placement == HudPlacement::HeadLocked);
        CHECK_FALSE(lower.hud.hidden);
    }

    TEST_CASE("activation distances: 0, or 0.6 to 2.0") {
        Settings s;
        s.interaction.terminal_distance = 0.3f;
        s.interaction.rewire_distance   = 3.0f;
        Settings t                      = sanitized(s);
        CHECK(t.interaction.terminal_distance == 0.6f);
        CHECK(t.interaction.rewire_distance == 2.0f);
        s.interaction.terminal_distance = -1.0f;
        s.interaction.rewire_distance   = kNaN;
        t                               = sanitized(s);
        CHECK(t.interaction.terminal_distance == 0.0f);
        CHECK(t.interaction.rewire_distance == 0.0f);
        s.interaction.terminal_distance = 1.0f;
        CHECK(sanitized(s).interaction.terminal_distance == 1.0f);
    }

    TEST_CASE("terminal text scale: the nearest listed step, NaN 1.0") {
        Settings s;
        const auto step = [&](float v) {
            s.interaction.terminal_text_scale = v;
            return sanitized(s).interaction.terminal_text_scale;
        };
        CHECK(step(1.3f) == 1.25f);
        CHECK(step(1.4f) == 1.5f);
        CHECK(step(3.0f) == 1.75f);
        CHECK(step(0.2f) == 1.0f);
        CHECK(step(kNaN) == 1.0f);
    }

    TEST_CASE("HUD panel values are clamped, NaN gives the default") {
        Settings s;
        s.hud.distance = 5.0f;
        s.hud.width    = 0.1f;
        s.hud.pitch    = -45.0f;
        Settings t     = sanitized(s);
        CHECK(t.hud.distance == 2.0f);
        CHECK(t.hud.width == 0.3f);
        CHECK(t.hud.pitch == -30.0f);
        s.hud.distance = kNaN;
        s.hud.width    = kNaN;
        s.hud.pitch    = kNaN;
        t              = sanitized(s);
        CHECK(t.hud.distance == 1.0f);
        CHECK(t.hud.width == 0.55f);
        CHECK(t.hud.pitch == 0.0f);
        s.hud.pitch = 20.0f;
        CHECK(sanitized(s).hud.pitch == 10.0f);
    }

    TEST_CASE("written into their own objects, enums as their names") {
        Settings s;
        s.hud.crosshair                = CrosshairMode::Off;
        s.interaction.camera_takeovers = CameraTakeovers::Reduced;
        json root                      = json::object();
        write_config(root, s);
        CHECK(root["Settings"]["Hud"]["Crosshair"] == "Off");
        CHECK(root["Settings"]["Hud"]["Placement"] == "HeadLocked");
        CHECK(root["Settings"]["Interaction"]["CameraTakeovers"] == "Reduced");
        CHECK(root["Settings"]["Interaction"]["FastAnimations"] == false);
    }

    TEST_CASE("commands: ranges, 0 for off, choices") {
        Settings s;
        using Kind = omvr::core::ui::CommandOutcome::Kind;
        CHECK(omvr::core::ui::apply_command(s, "mvr_terminaldistance", "0.5").kind == Kind::Refused);
        CHECK(omvr::core::ui::apply_command(s, "mvr_terminaldistance", "1.0").kind == Kind::Applied);
        CHECK(s.interaction.terminal_distance == 1.0f);
        CHECK(omvr::core::ui::apply_command(s, "mvr_terminaldistance", "0").kind == Kind::Applied);
        CHECK(s.interaction.terminal_distance == 0.0f);
        CHECK(omvr::core::ui::apply_command(s, "mvr_hudpitch", "-15").kind == Kind::Applied);
        CHECK(s.hud.pitch == -15.0f);
        CHECK(omvr::core::ui::apply_command(s, "mvr_hudpitch", "15").kind == Kind::Refused);
        CHECK(omvr::core::ui::apply_command(s, "mvr_crosshair", "2").kind == Kind::Applied);
        CHECK(s.hud.crosshair == CrosshairMode::Auto);
        CHECK(omvr::core::ui::apply_command(s, "mvr_crosshair", "3").kind == Kind::Refused);
        CHECK(omvr::core::ui::apply_command(s, "mvr_hidehud", "1").kind == Kind::Applied);
        CHECK(s.hud.hidden);
    }
}

TEST_SUITE("HUD hidden by the player") {
    TEST_CASE("Hud.Hidden false: the session starts shown and nothing is touched") {
        HudVisibility v;
        v.start(false);
        CHECK(v.idle(false));
        const auto step = v.update(false, movies(false, false));
        CHECK(step.actions[0] == HudAction::None);
        CHECK(step.actions[1] == HudAction::None);
        CHECK_FALSE(step.hidden);
    }

    TEST_CASE("Hud.Hidden true hides both objects once they exist") {
        HudVisibility v;
        v.start(true);
        CHECK_FALSE(v.idle(true));
        HudMovieStates none{};
        auto step = v.update(true, none);
        CHECK(step.actions[0] == HudAction::None);
        step = v.update(true, movies(false, false));
        CHECK(step.actions[0] == HudAction::Hide);
        CHECK(step.actions[1] == HudAction::Hide);
        step = v.update(true, movies(true, true));
        CHECK(step.actions[0] == HudAction::None);
        CHECK(step.actions[1] == HudAction::None);
    }

    TEST_CASE("the gesture shows what the mod hid, and hides again") {
        HudVisibility v;
        v.start(true);
        v.update(true, movies(false, false));
        v.request_toggle();
        auto step = v.update(true, movies(true, true));
        CHECK(step.toggled);
        CHECK_FALSE(step.hidden);
        CHECK(step.actions[0] == HudAction::Show);
        CHECK(step.actions[1] == HudAction::Show);
        CHECK(v.idle(true));
        v.request_toggle();
        step = v.update(true, movies(false, false));
        CHECK(step.hidden);
        CHECK(step.actions[0] == HudAction::Hide);
    }

    TEST_CASE("a HUD the game hides is never shown by the mod") {
        HudVisibility v;
        v.start(true);
        v.update(true, movies(false, false));
        // A scripted scene hides the HUD on top of the player's hide.
        v.on_game_hide();
        v.request_toggle();
        auto step = v.update(true, movies(true, true));
        CHECK_FALSE(step.hidden);
        CHECK(step.actions[0] == HudAction::None);
        CHECK(step.actions[1] == HudAction::None);
        // Visible state the game hides for itself from the start.
        HudVisibility w;
        w.start(false);
        w.request_toggle();
        w.update(false, movies(true, true));
        w.request_toggle();
        step = w.update(false, movies(true, true));
        CHECK(step.actions[0] == HudAction::None);
    }

    TEST_CASE("while hidden, the HUD the game shows again is hidden at the next update") {
        HudVisibility v;
        v.start(true);
        v.update(true, movies(false, false));
        v.on_game_hide();
        v.update(true, movies(true, true));
        v.on_game_show();
        const auto step = v.update(true, movies(false, false));
        CHECK(step.actions[0] == HudAction::Hide);
        CHECK(step.actions[1] == HudAction::Hide);
    }

    TEST_CASE("changing Hud.Hidden on the page sets the state at once") {
        HudVisibility v;
        v.start(false);
        CHECK_FALSE(v.idle(true));
        auto step = v.update(true, movies(false, false));
        CHECK(step.hidden);
        CHECK(step.actions[0] == HudAction::Hide);
        step = v.update(false, movies(true, true));
        CHECK_FALSE(step.hidden);
        CHECK(step.actions[0] == HudAction::Show);
    }
}

TEST_SUITE("crosshair") {
    TEST_CASE("which modes hide it") {
        using omvr::core::ui::crosshair_hidden;
        CHECK_FALSE(crosshair_hidden(CrosshairMode::On, true));
        CHECK(crosshair_hidden(CrosshairMode::Off, false));
        CHECK(crosshair_hidden(CrosshairMode::Auto, true));
        CHECK_FALSE(crosshair_hidden(CrosshairMode::Auto, false));
    }

    TEST_CASE("hidden: the game's show is skipped, its hide not replayed") {
        CrosshairGate g;
        auto b = g.before_update(true, false);
        CHECK_FALSE(b.hide_now);
        CHECK_FALSE(g.allow_show(true));
        CHECK(g.hidden_by_mod());
        // The game now decides "hidden": the movie already is.
        CHECK_FALSE(g.allow_hide());
        CHECK_FALSE(g.hidden_by_mod());
        CHECK(g.allow_hide());
    }

    TEST_CASE("showing when the setting changes: hidden within one update") {
        CrosshairGate g;
        const auto b = g.before_update(true, true);
        CHECK(b.hide_now);
        CHECK_FALSE(b.clear_cache);
        CHECK(g.before_update(true, true).hide_now == false);
    }

    TEST_CASE("back on: the cache is cleared so the game shows it when it wants") {
        CrosshairGate g;
        g.before_update(true, true);
        auto b = g.before_update(false, true);
        CHECK(b.clear_cache);
        CHECK(g.allow_show(false));
        b = g.before_update(false, true);
        CHECK_FALSE(b.clear_cache);
    }
}

TEST_SUITE("HUD toggle gesture") {
    using omvr::core::input::Controls;

    Controls at_head(bool map, float left_distance) {
        Controls c;
        c.map                 = map;
        c.poses.head          = {0.0f, 1.6f, 0.0f};
        c.poses.head_valid    = true;
        c.poses.hands[0]      = {left_distance, 1.6f, 0.0f};
        c.poses.hand_valid[0] = true;
        c.poses.focused       = true;
        return c;
    }

    TEST_CASE("left hand within 0.20 m and a Map press flips the HUD, Map does not reach the map") {
        HudToggleGesture g;
        const Clock::time_point t0{10s};
        g.update(true, at_head(false, 0.1f), t0);
        const auto r = g.update(true, at_head(true, 0.1f), t0 + 1s);
        CHECK(r.fired);
        CHECK(r.take_map);
        // Held: taken until released, fires once.
        const auto held = g.update(true, at_head(true, 0.5f), t0 + 1100ms);
        CHECK_FALSE(held.fired);
        CHECK(held.take_map);
        CHECK_FALSE(g.update(true, at_head(false, 0.1f), t0 + 1200ms).take_map);
    }

    TEST_CASE("away from the head, or off, Map is the map's") {
        HudToggleGesture g;
        const Clock::time_point t0{10s};
        g.update(true, at_head(false, 0.5f), t0);
        auto r = g.update(true, at_head(true, 0.5f), t0 + 1s);
        CHECK_FALSE(r.fired);
        CHECK_FALSE(r.take_map);
        // A press begun away stays the map's when the hand comes near.
        r = g.update(true, at_head(true, 0.1f), t0 + 1100ms);
        CHECK_FALSE(r.take_map);
        HudToggleGesture off;
        off.update(false, at_head(false, 0.1f), t0);
        r = off.update(false, at_head(true, 0.1f), t0 + 1s);
        CHECK_FALSE(r.fired);
        CHECK_FALSE(r.take_map);
    }

    TEST_CASE("not in the first 0.5 s of input focus") {
        HudToggleGesture g;
        const Clock::time_point t0{10s};
        g.update(true, at_head(false, 0.1f), t0);
        auto r = g.update(true, at_head(true, 0.1f), t0 + 400ms);
        CHECK_FALSE(r.fired);
        CHECK(r.suppressed_focus);
        CHECK(r.take_map);
        g.update(true, at_head(false, 0.1f), t0 + 450ms);
        r = g.update(true, at_head(true, 0.1f), t0 + 600ms);
        CHECK(r.fired);
    }
}

TEST_SUITE("HUD placement") {
    TEST_CASE("the defaults give v1's panel exactly") {
        const Matrix44 view = view_towards(0.7f);
        const HudPanel p    = omvr::core::ui::hud_panel(omvr::core::config::HudSettings{});
        CHECK(p.is_v1());
        CHECK(omvr::core::ui::head_locked_quad(view, 0.5f, p) ==
              omvr::core::stereo::hud_panel_quad(view, 0.5f));
        HudPlacer placer;
        CHECK(placer.place(view, 0.5f, p, Clock::time_point{10s}) ==
              omvr::core::stereo::hud_panel_quad(view, 0.5f));
    }

    TEST_CASE("head-locked: distance, width and pitch") {
        Matrix44 view{};
        omvr::util::identity(view.data());
        HudPanel p;
        p.distance       = 2.0f;
        p.width          = 0.8f;
        p.pitch          = -30.0f * kDeg;
        const Matrix44 q = omvr::core::ui::head_locked_quad(view, 0.5f, p);
        CHECK(q[0] == doctest::Approx(0.8f)); // right, full width
        CHECK(q[12] == doctest::Approx(0.0f));
        CHECK(q[13] == doctest::Approx(-1.0f)); // 2 m * sin(-30°): below the view
        CHECK(q[14] == doctest::Approx(2.0f * std::cos(30.0f * kDeg)));
        CHECK(q[9] == doctest::Approx(-0.5f)); // facing the eyes, tilted with the pitch
        CHECK(q[5] == doctest::Approx(0.8f * 0.5f * std::cos(30.0f * kDeg)));
    }

    TEST_CASE("follow: level, still for small glances, follows a turn beyond 20 degrees over 0.3 s") {
        HudPlacer placer;
        HudPanel p;
        p.placement = HudPlacement::Follow;
        const Clock::time_point t0{10s};
        Matrix44 q = placer.place(view_towards(0.0f), 0.5f, p, t0);
        CHECK(panel_heading(q) == doctest::Approx(0.0f));
        CHECK(q[9] == doctest::Approx(0.0f)); // level
        q = placer.place(view_towards(15.0f * kDeg), 0.5f, p, t0 + 100ms);
        CHECK(panel_heading(q) == doctest::Approx(0.0f));
        q = placer.place(view_towards(40.0f * kDeg), 0.5f, p, t0 + 200ms);
        CHECK(panel_heading(q) == doctest::Approx(0.0f)); // the move starts here
        q = placer.place(view_towards(40.0f * kDeg), 0.5f, p, t0 + 350ms);
        CHECK(panel_heading(q) == doctest::Approx(20.0f * kDeg)); // half way, eased
        q = placer.place(view_towards(40.0f * kDeg), 0.5f, p, t0 + 500ms);
        CHECK(panel_heading(q) == doctest::Approx(40.0f * kDeg));
    }

    TEST_CASE("follow: a snap turn or recenter re-places at once; straight down keeps the heading") {
        HudPlacer placer;
        HudPanel p;
        p.placement = HudPlacement::Follow;
        const Clock::time_point t0{10s};
        placer.place(view_towards(0.0f), 0.5f, p, t0);
        placer.request_replace();
        Matrix44 q = placer.place(view_towards(90.0f * kDeg), 0.5f, p, t0 + 10ms);
        CHECK(panel_heading(q) == doctest::Approx(90.0f * kDeg));
        Matrix44 down{};
        omvr::util::identity(down.data());
        down[10] = 0.0f;
        down[6]  = -1.0f;
        q        = placer.place(down, 0.5f, p, t0 + 20ms);
        CHECK(panel_heading(q) == doctest::Approx(90.0f * kDeg));
        for (float v : q) CHECK(std::isfinite(v));
    }

    TEST_CASE("follow: a re-place requested while looking straight down waits for a heading") {
        HudPlacer placer;
        HudPanel p;
        p.placement = HudPlacement::Follow;
        const Clock::time_point t0{10s};
        placer.place(view_towards(0.0f), 0.5f, p, t0);
        Matrix44 down{};
        omvr::util::identity(down.data());
        down[10] = 0.0f;
        down[6]  = -1.0f;
        placer.request_replace();
        placer.place(down, 0.5f, p, t0 + 10ms); // no heading: the request is kept
        const Matrix44 q = placer.place(view_towards(90.0f * kDeg), 0.5f, p, t0 + 20ms);
        CHECK(panel_heading(q) == doctest::Approx(90.0f * kDeg)); // at once, no glide
    }
}

TEST_SUITE("UI patch variants") {
    using omvr::core::ui::PatchEntry;
    using omvr::tests::bytes_of;
    using omvr::tests::string_of;

    class Writer {
    public:
        explicit Writer(std::uint32_t count) {
            text("MVRUIP01");
            u32(4);
            u32(count);
        }
        void stock(std::string_view name, std::string_view data) {
            entry(name, PatchEntry::Stock);
            u32(static_cast<std::uint32_t>(data.size()));
            u32(omvr::util::crc32(bytes_of(data)));
        }
        void added(std::string_view name, std::string_view data) {
            entry(name, PatchEntry::New);
            u32(static_cast<std::uint32_t>(data.size()));
            text(data);
        }
        std::vector<std::byte> out;

    private:
        void entry(std::string_view name, PatchEntry kind) {
            const auto n = static_cast<std::uint16_t>(name.size());
            raw(&n, sizeof(n));
            text(name);
            out.push_back(static_cast<std::byte>(kind));
        }
        void u32(std::uint32_t v) { raw(&v, sizeof(v)); }
        void text(std::string_view s) { raw(s.data(), s.size()); }
        void raw(const void* p, std::size_t n) {
            const auto* b = static_cast<const std::byte*>(p);
            out.insert(out.end(), b, b + n);
        }
    };

    TEST_CASE("the terminal text steps' tags and keys") {
        using omvr::core::ui::terminal_text_variant;
        CHECK(terminal_text_variant(1.0f).tag.empty());
        CHECK(terminal_text_variant(1.0f).key == 0);
        CHECK(terminal_text_variant(1.25f).tag == "text125");
        CHECK(terminal_text_variant(1.5f).tag == "text150");
        CHECK(terminal_text_variant(1.75f).tag == "text175");
        CHECK(terminal_text_variant(1.75f).key == 3);
    }

    TEST_CASE("a variant replaces its entry only when asked for") {
        omvr::tests::FakeArchiveFiles files;
        omvr::tests::RecordingLog log;
        files.files[L"UI.PAK"] = omvr::core::ui::build_pak(4, {{"T.GFX", bytes_of("stock")}});
        Writer w(3);
        w.stock("T.GFX", "stock");
        w.added("T.GFX|text150", "large");
        w.added("T.GFX|text175", "huge!");

        REQUIRE(omvr::core::ui::build_ui_archive(files, log, L"UI.PAK", w.out, L"A.PAK"));
        auto built = omvr::core::ui::read_pak(files.files.at(L"A.PAK"));
        REQUIRE(built.has_value());
        CHECK(built->size() == 1);
        CHECK(string_of(built->at("T.GFX")) == "stock");

        REQUIRE(omvr::core::ui::build_ui_archive(files, log, L"UI.PAK", w.out, L"B.PAK", "text150"));
        built = omvr::core::ui::read_pak(files.files.at(L"B.PAK"));
        REQUIRE(built.has_value());
        CHECK(built->size() == 1);
        CHECK(string_of(built->at("T.GFX")) == "large");
        CHECK(log.contains("ui patch: variant text150 built in"));

        REQUIRE(omvr::core::ui::build_ui_archive(files, log, L"UI.PAK", w.out, L"C.PAK", "text125"));
        CHECK(string_of(omvr::core::ui::read_pak(files.files.at(L"C.PAK"))->at("T.GFX")) == "stock");
        CHECK(log.contains("ui patch: the embedded patch has no variant text125"));
    }
}
