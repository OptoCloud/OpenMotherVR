#pragma once

#include "core/config/settings.h"
#include "core/config/settings_store.h"
#include "core/config/settings_table.h"
#include "core/diag/ports/log.h"
#include "core/ui/display_reset.h"
#include "core/ui/ports/game_signals.h"

#include <optional>
#include <string>
#include <string_view>

namespace omvr::core::ui {

// A setting a command changed, for the log: its key path and new value.
struct SettingChange {
    std::string_view path;
    std::string      text;
};

// What an mvr_ command did to the settings.
struct CommandOutcome {
    enum class Kind { Unknown, Refused, Applied };
    Kind                         kind   = Kind::Unknown;
    std::optional<SettingChange> change = std::nullopt; // set when the value differs
    config::CommandEffect        effect = config::CommandEffect::None;
};

// Whether the console input `cmd` is an options-page command (kCommandPrefix
// after any leading spaces); every other command is the game's.
bool is_mod_command(std::string_view cmd) noexcept;

// Applies the mvr_ command `name` with its value token `value` (null when
// there is none) to `cfg`, by the settings table's rule for it. Pure: the
// caller logs, saves and carries out the effect.
CommandOutcome apply_command(config::Settings& cfg, std::string_view name, const char* value);

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
    Commands(config::SettingsStore& store, DisplayReset& display_reset, ports::GameSignals& signals,
             diag::ports::Log& log) noexcept
        : store_(store), display_reset_(display_reset), signals_(signals), log_(log) {}

    // Runs the mvr_ command `cmd` (the raw console input, its first
    // whitespace-terminated token the command name), tokenising it in place.
    void run(char* cmd) noexcept;

private:
    // Saves, rebuilds and quits as an accepted command requires.
    void carry_out(const CommandOutcome& outcome);

    config::SettingsStore& store_;
    DisplayReset&          display_reset_;
    ports::GameSignals&    signals_;
    diag::ports::Log&      log_;
};

} // namespace omvr::core::ui
