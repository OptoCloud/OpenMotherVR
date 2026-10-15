#pragma once

#include "core/ui/display_reset.h"
#include "core/ui/ports/game_signals.h"

namespace omvr::core::ui {

// The options page's console commands ("mvr_*"): each sets one setting,
// saves mothervr.json and, for the runtime, quits the game. Game thread.
//
// One rule for bad values (spec 06 §4.2): a missing value, one that is not a
// number of its kind, one outside the accepted set, or an unknown mvr_ name
// changes nothing: no setting, no quit, no rebuild, no save. The original
// read a missing or malformed value as 0 (atoi), so a hand-typed or
// truncated command silently turned a switch off or quit the game.
//
// From: FUN_100202e0 @ 0x100202e0 (size 1418 bytes).
class Commands {
public:
    Commands(DisplayReset& display_reset, ports::GameSignals& signals) noexcept
        : display_reset_(display_reset), signals_(signals) {}

    // Runs the mvr_ command `cmd` (the raw console input, its first
    // whitespace-terminated token the command name), tokenising it in place.
    void run(char* cmd) noexcept;

private:
    DisplayReset&       display_reset_;
    ports::GameSignals& signals_;
};

} // namespace omvr::core::ui
