// OpenMotherVR-Provenance: clean
// core/ui: the level a file path names, the play state and the Discord
// activity text.

#include "core/ui/play_state.h"
#include "core/ui/presence_text.h"

#include <doctest/doctest.h>

#include <ostream>

using namespace omvr::core::ui;
using omvr::core::input::Interaction;

TEST_CASE("level_folder finds the level of a file under ENV\\PRODUCTION") {
    CHECK(level_folder(R"(data\ENV\PRODUCTION\TECH_HUB\RENDERABLE\models_level.bin)") == "TECH_HUB");
    CHECK(level_folder("DATA/env/production/sci_hub/WORLD/x.bin") == "sci_hub");
    CHECK(level_folder(R"(DATA\ENV\PRODUCTION\DLC\CHALLENGEMAP3\WORLD\x.bin)") == "CHALLENGEMAP3");
    CHECK(level_folder(R"(DATA\ENV\GLOBAL\x.bin)").empty());
    CHECK(level_folder(R"(DATA\ENV\PRODUCTION\readme.txt)").empty());
    CHECK(level_folder(R"(DATA\UI.PAK)").empty());
    CHECK(level_folder(R"(ENV\PRODUCTION\SOLACE\WORLD\x.bin)") == "SOLACE");
}

TEST_CASE("find_level is case-insensitive and knows the DLC") {
    CHECK(find_level("frontend") == 0);
    CHECK(levels()[static_cast<std::size_t>(find_level("ENG_ALIEN_NEST"))].name == "The Nest");
    CHECK(levels()[static_cast<std::size_t>(find_level("BSPNOSTROMO_TWOTEAMS"))].name ==
          "Nostromo: Crew Expendable");
    CHECK(find_level("NOT_A_LEVEL") == -1);
}

TEST_CASE("PlayState follows level files, menus and cutscenes") {
    PlayState s;
    CHECK(s.level() == -1);
    s.on_file_open(R"(data\ENV\PRODUCTION\TECH_COMMS\WORLD\a.bin)");
    CHECK(s.level() == find_level("TECH_COMMS"));
    s.on_file_open(R"(data\ENV\GLOBAL\b.bin)");
    CHECK(s.level() == find_level("TECH_COMMS"));
    s.on_event("cutscene_visibility_start");
    CHECK(s.cutscene());
    s.set_screen(Screen::Loading);
    CHECK_FALSE(s.cutscene());
    CHECK(s.screen() == Screen::Loading);
}

TEST_CASE("describe: where and what, in priority order") {
    const int nest = find_level("ENG_ALIEN_NEST");
    PresenceInput in{.level = nest};
    CHECK(describe(in).details == "The Nest");
    CHECK(describe(in).state == "Exploring");
    in.hiding = true;
    CHECK(describe(in).state == "Hiding");
    in.interaction = Interaction::Hacking;
    CHECK(describe(in).state == "Hacking a lock");
    in.interaction = Interaction::Generic;
    CHECK(describe(in).state == "Hiding");
    in.cutscene = true;
    CHECK(describe(in).state == "Watching a cutscene");
    in.screen = Screen::Pause;
    CHECK(describe(in).state == "Paused");
    in.screen = Screen::Loading;
    CHECK(describe(in).state == "Loading");
    CHECK(describe(in).details == "The Nest");
}

TEST_CASE("describe: the main menu and an untracked game") {
    PresenceInput in{};
    CHECK(describe(in).details == "Main menu");
    CHECK(describe(in).state.empty());
    in.level  = find_level("TECH_HUB");
    in.screen = Screen::Title;
    CHECK(describe(in).details == "Main menu");
    in.tracked = false;
    CHECK(describe(in).details == "VR off");
}
