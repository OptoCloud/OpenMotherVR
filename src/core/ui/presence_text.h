// OpenMotherVR-Provenance: clean
#pragma once

#include "core/input/interaction_state.h"
#include "core/ui/play_state.h"

#include <string_view>

// The two lines of the Discord activity, from what the player is doing.
// Pure.
namespace omvr::core::ui {

struct PresenceInput {
    bool tracked                   = true; // false: the game state is not watched (runtime None)
    int level                      = -1;   // index into levels(), -1 before the first load
    Screen screen                  = Screen::None;
    bool cutscene                  = false;
    input::Interaction interaction = input::Interaction::None;
    bool hiding                    = false;
};

struct PresenceText {
    std::string_view details; // where: the level, or the main menu
    std::string_view state;   // what: loading, paused, hiding, hacking...
};

// Static strings only; nothing is allocated.
PresenceText describe(const PresenceInput& in) noexcept;

} // namespace omvr::core::ui
