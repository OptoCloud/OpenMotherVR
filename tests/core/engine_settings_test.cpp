// OpenMotherVR-Provenance: clean
// core/render/engine_settings: the engine settings override (spec 24 §6) on
// small synthetic XML written for these tests, never the game's file.
// Expected values come from spec 24 §6.2 and §10.

#include "core/config/settings_json.h"
#include "core/config/settings_table.h"
#include "core/render/engine_settings.h"
#include "fakes/fakes.h"

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using namespace omvr::core::render;
using omvr::core::config::GraphicsSettings;
using omvr::tests::MemoryFileStore;
using omvr::tests::RecordingLog;

// A file shaped like a settings file: CRLF lines, tab indentation, an
// option with children, a self-closing one, comments.
constexpr std::string_view kSettings =
    "<?xml version=\"1.0\" encoding=\"us-ascii\"?>\r\n"
    "<!-- test file -->\r\n"
    "<Root>\r\n"
    "\t<Settings>\r\n"
    "\t\t<Setting name=\"ShadowMapResolution\">\r\n"
    "\t\t\t<Quality name=\"512\"\tint=\"512\"\tprecedence=\"1\"/>\r\n"
    "\t\t\t<Quality name=\"2048\"\tint=\"2048\"\tprecedence=\"4\">\r\n"
    "\t\t\t\t<DefaultRequirements><Gpu value=\"7.6\"/></DefaultRequirements>\r\n"
    "\t\t\t</Quality>\r\n"
    "\t\t</Setting>\r\n"
    "\t\t<Setting name=\"Planar Reflections\">\r\n"
    "\t\t\t<Quality name=\"Off\" int=\"0\" precedence=\"1\"/>\r\n"
    "\t\t\t<Quality name=\"On\" int=\"3\" precedence=\"2\"/>\r\n"
    "\t\t</Setting>\r\n"
    "\t\t<Setting name=\"Streaming Texture Heap Size\">\r\n"
    "\t\t\t<Quality name=\"Default\" int=\"300\" precedence=\"1\"/>\r\n"
    "\t\t\t<Quality name=\"Big\" int=\"300\" precedence=\"2\"/>\r\n"
    "\t\t</Setting>\r\n"
    "\t</Settings>\r\n"
    "</Root>\r\n";

std::string inserted_after(std::string_view xml, std::string_view anchor, std::string_view added) {
    std::string out(xml);
    const auto at = out.find(anchor);
    REQUIRE(at != std::string::npos);
    out.insert(at + anchor.size(), added);
    return out;
}

GraphicsSettings with(bool shadows, bool reflections, int texture_mb = 0, int model_mb = 0) {
    GraphicsSettings g;
    g.engine_settings_override  = true;
    g.shadow_map_4096           = shadows;
    g.sharp_reflections         = reflections;
    g.streaming_texture_heap_mb = texture_mb;
    g.streaming_model_heap_mb   = model_mb;
    return g;
}

TEST_CASE("engine options: none at the v1 defaults, the 24 §6.2 ones per key") {
    CHECK(engine_options(GraphicsSettings{}).empty());

    const auto all = engine_options(with(true, true, 600, 256));
    REQUIRE(all.size() == 4);
    CHECK(all[0].setting == "ShadowMapResolution");
    CHECK(all[0].name == "4096");
    CHECK(all[0].value == 4096);
    CHECK(all[0].rank == OptionRank::Lowest);
    CHECK(all[1].setting == "Planar Reflections");
    CHECK(all[1].name == "Alternative");
    CHECK(all[1].value == 6);
    CHECK(all[1].rank == OptionRank::Lowest);
    CHECK(all[2].setting == "Streaming Texture Heap Size");
    CHECK(all[2].name == "600 MB");
    CHECK(all[2].value == 600);
    CHECK(all[2].rank == OptionRank::Highest);
    CHECK(all[3].setting == "Streaming Model Heap Size");
    CHECK(all[3].value == 256);
}

TEST_CASE("engine settings: 4096 and Alternative are inserted, every other byte kept") {
    const auto options = engine_options(with(true, true));
    const auto edit    = add_engine_options(kSettings, options);
    REQUIRE(edit.well_formed);
    REQUIRE(edit.results.size() == 2);
    CHECK(edit.results[0].outcome == AddOutcome::Added);
    CHECK(edit.results[1].outcome == AddOutcome::Added);
    // Never the default: below the lowest precedence, not below 0.
    CHECK(edit.results[0].precedence == 0);
    CHECK(edit.results[1].precedence == 0);
    CHECK(edit.added() == 2);

    std::string expected = inserted_after(kSettings, "\t\t\t</Quality>",
                                          "\r\n\t\t\t<Quality name=\"4096\" int=\"4096\" precedence=\"0\"/>");
    expected = inserted_after(expected, "<Quality name=\"On\" int=\"3\" precedence=\"2\"/>",
                              "\r\n\t\t\t<Quality name=\"Alternative\" int=\"6\" precedence=\"0\"/>");
    CHECK(edit.text == expected);
}

TEST_CASE("engine settings: LF files get LF lines") {
    constexpr std::string_view lf =
        "<S>\n  <Setting name=\"Planar Reflections\">\n"
        "    <Quality name=\"Off\" int=\"0\" precedence=\"3\"/>\n  </Setting>\n</S>\n";
    const auto edit = add_engine_options(lf, engine_options(with(false, true)));
    CHECK(edit.text ==
          "<S>\n  <Setting name=\"Planar Reflections\">\n"
          "    <Quality name=\"Off\" int=\"0\" precedence=\"3\"/>\n"
          "    <Quality name=\"Alternative\" int=\"6\" precedence=\"2\"/>\n  </Setting>\n</S>\n");
}

TEST_CASE("engine settings: a missing setting is skipped, the rest applies") {
    constexpr std::string_view only_shadows =
        "<R><Setting name=\"ShadowMapResolution\">"
        "<Quality name=\"512\" int=\"512\" precedence=\"2\"/>"
        "</Setting><Setting name=\"Planar Reflections\"/></R>";
    const auto edit = add_engine_options(only_shadows, engine_options(with(true, true)));
    REQUIRE(edit.well_formed);
    CHECK(edit.results[0].outcome == AddOutcome::Added);
    CHECK(edit.results[0].precedence == 1);
    CHECK(edit.results[1].outcome == AddOutcome::NoSetting); // it has no options
    CHECK(edit.text.find("Alternative") == std::string::npos);
    CHECK(edit.text.find(R"(<Quality name="4096" int="4096" precedence="1"/>)") != std::string::npos);
}

TEST_CASE("engine settings: an option already there is not added again") {
    constexpr std::string_view has =
        "<R><Setting name=\"ShadowMapResolution\">"
        "<Quality name=\"Max\" int=\"4096\" precedence=\"5\"/></Setting>"
        "<Setting name=\"Planar Reflections\">"
        "<Quality name=\"Alternative\" int=\"1\" precedence=\"1\"/></Setting></R>";
    const auto edit = add_engine_options(has, engine_options(with(true, true)));
    REQUIRE(edit.well_formed);
    CHECK(edit.results[0].outcome == AddOutcome::AlreadyOffered); // same value
    CHECK(edit.results[1].outcome == AddOutcome::AlreadyOffered); // same name
    CHECK(edit.added() == 0);
    CHECK(edit.text.empty());
}

TEST_CASE("engine settings: a heap size becomes the highest precedence") {
    const auto bigger = add_engine_options(kSettings, engine_options(with(false, false, 600)));
    REQUIRE(bigger.results.size() == 1);
    CHECK(bigger.results[0].outcome == AddOutcome::Added);
    CHECK(bigger.results[0].precedence == 3);
    CHECK(bigger.text.find("<Quality name=\"Big\" int=\"300\" precedence=\"2\"/>\r\n"
                           "\t\t\t<Quality name=\"600 MB\" int=\"600\" precedence=\"3\"/>\r\n") !=
          std::string::npos);

    // The default already has the size: nothing to add.
    const auto same = add_engine_options(kSettings, engine_options(with(false, false, 300)));
    CHECK(same.results[0].outcome == AddOutcome::AlreadyOffered);

    // A file without the model heap setting.
    const auto model = add_engine_options(kSettings, engine_options(with(false, false, 0, 256)));
    CHECK(model.results[0].outcome == AddOutcome::NoSetting);
}

TEST_CASE("engine settings: a file that is not well-formed gets nothing") {
    for (const std::string_view bad : {
             std::string_view{"<R><Setting name=\"ShadowMapResolution\"><Quality name=\"512\" int=\"512\"/>"},
             std::string_view{"<R><Setting name=\"a\"></Quality></R>"},
             std::string_view{"<R a=\"1\" a=\"2\"/>"},
             std::string_view{"<R a=1/>"},
             std::string_view{"<R/><R/>"},
             std::string_view{"text<R/>"},
             std::string_view{"<R><!-- unclosed </R>"},
             std::string_view{"<R>a & b</R>"},
             std::string_view{"\xFF\xFE<\0R\0/\0>\0", 10}, // UTF-16
             std::string_view{""},
         }) {
        const auto edit = add_engine_options(bad, engine_options(with(true, true)));
        CHECK_FALSE(edit.well_formed);
        CHECK(edit.text.empty());
        CHECK(edit.results.empty());
    }
    // A byte-order mark, a DOCTYPE, CDATA and references are accepted.
    constexpr std::string_view fine =
        "\xEF\xBB\xBF<!DOCTYPE R [<!ENTITY x \"y\">]><R>&amp;&#65;<![CDATA[<>]]>"
        "<Setting name='Planar Reflections'><Quality name='Off' int='0'/>"
        "</Setting></R>";
    const auto edit = add_engine_options(fine, engine_options(with(false, true)));
    CHECK(edit.well_formed);
    CHECK(edit.added() == 1);
}

TEST_CASE("engine settings: the game's path is matched however it is spelt") {
    constexpr std::string_view game = "DATA\\engine_settings.xml";
    CHECK(names_engine_settings("DATA\\engine_settings.xml", game));
    CHECK(names_engine_settings("data/ENGINE_SETTINGS.XML", game));
    CHECK(names_engine_settings("C:\\Games\\Alien\\DATA\\ENGINE_SETTINGS.XML", game));
    CHECK_FALSE(names_engine_settings("C:\\Games\\MYDATA\\engine_settings.xml", game));
    CHECK_FALSE(names_engine_settings("DATA\\engine_settings_savefile.bin", game));
    CHECK_FALSE(names_engine_settings("engine_settings.xml", game));
    CHECK_FALSE(names_engine_settings("DATA\\engine_settings.xml", ""));
}

TEST_CASE("engine settings copy: built from the player's file, which is never written") {
    MemoryFileStore files;
    RecordingLog log;
    const std::filesystem::path player = "game/DATA/ENGINE_SETTINGS.XML";
    const std::filesystem::path copy   = "game/openmothervr_engine_settings.xml";
    files.files[player]                = std::string(kSettings);
    const auto options                 = engine_options(with(true, true, 0, 256));

    CHECK(build_engine_settings_copy(files, log, player, copy, options));
    CHECK(files.files[player] == kSettings);
    CHECK(files.writes == 1);
    CHECK(files.files[copy] == add_engine_options(kSettings, options).text);
    CHECK(log.contains("override on, 3 addition(s)"));
    CHECK(log.contains(R"(added "4096" (int 4096, precedence 0) to ShadowMapResolution)"));
    CHECK(log.contains(R"(added "Alternative" (int 6, precedence 0) to Planar Reflections)"));
    CHECK(log.contains(R"(skipped "256 MB": the file has no Streaming Model Heap Size options)"));
}

TEST_CASE("engine settings copy: the game reads its own file when nothing can be added") {
    MemoryFileStore files;
    RecordingLog log;
    const std::filesystem::path player = "DATA/ENGINE_SETTINGS.XML";
    const std::filesystem::path copy   = "openmothervr_engine_settings.xml";
    const auto options                 = engine_options(with(true, true));

    CHECK_FALSE(build_engine_settings_copy(files, log, player, copy, options)); // unreadable
    CHECK(log.contains("cannot read"));

    files.files[player] = "<R><Broken></R>";
    CHECK_FALSE(build_engine_settings_copy(files, log, player, copy, options));
    CHECK(log.contains("not well-formed XML"));

    files.files[player] = "<R/>";
    CHECK_FALSE(build_engine_settings_copy(files, log, player, copy, options));
    CHECK(log.contains("nothing added"));
    CHECK(files.writes == 0);
}

TEST_CASE("Graphics settings: defaults, ranges and the file (spec 24 §10)") {
    using namespace omvr::core::config;
    const Settings defaults{};
    CHECK_FALSE(defaults.graphics.engine_settings_override);
    CHECK_FALSE(defaults.graphics.shadow_map_4096);
    CHECK_FALSE(defaults.graphics.sharp_reflections);
    CHECK(defaults.graphics.streaming_texture_heap_mb == 0);
    CHECK(defaults.graphics.streaming_model_heap_mb == 0);

    Settings s;
    for (const auto& [in, texture, model] : std::vector<std::tuple<int, int, int>>{
             {0, 0, 0}, {-5, 0, 0}, {1, 300, 128}, {350, 350, 350}, {5000, 1024, 512}}) {
        s.graphics.streaming_texture_heap_mb = in;
        s.graphics.streaming_model_heap_mb   = in;
        sanitize(s);
        CHECK(s.graphics.streaming_texture_heap_mb == texture);
        CHECK(s.graphics.streaming_model_heap_mb == model);
    }

    nlohmann::json root          = nlohmann::json::object();
    root["Settings"]["Graphics"] = {{"EngineSettingsOverride", true},
                                    {"ShadowMap4096", true},
                                    {"SharpReflections", "yes"},
                                    {"StreamingTextureHeapMB", 700},
                                    {"Unknown", 1}};
    const Settings read          = read_config(root);
    CHECK(read.graphics.engine_settings_override);
    CHECK(read.graphics.shadow_map_4096);
    CHECK_FALSE(read.graphics.sharp_reflections); // non-bool: default
    CHECK(read.graphics.streaming_texture_heap_mb == 700);
    write_config(root, read);
    CHECK(root["Settings"]["Graphics"]["Unknown"] == 1);
    CHECK(root["Settings"]["Graphics"]["StreamingModelHeapMB"] == 0);
}

} // namespace
