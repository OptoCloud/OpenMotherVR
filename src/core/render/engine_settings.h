// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/ports/file_store.h"
#include "core/config/settings.h"
#include "core/diag/ports/log.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The engine settings override (24 §6): the game's graphics options come
// from the player's own DATA\ENGINE_SETTINGS.XML, read once at start-up; with
// Graphics.EngineSettingsOverride on, the game reads a copy of that file
// with the VR options of 24 §6.2 added, and nothing else changed. The
// player's file is never written.
namespace omvr::core::render {

// Where a new option ranks among its setting's options. The game picks the
// highest precedence whose requirements hold as the default of a new
// settings file (24 §1.9).
enum class OptionRank {
    Lowest,  // never the default: one below the lowest precedence (not below 0)
    Highest, // the default: one above the highest
};

// One option added to a setting: a new `Quality` element inside the
// `Setting` element whose name attribute is `setting`.
struct EngineOption {
    std::string_view setting; // the Setting's name
    std::string name;         // the Quality's name: what the Video page shows
    int value;                // its int attribute
    OptionRank rank;
};

// The options 24 §6.2 adds for `graphics`, in the order the keys are listed
// there; empty when no addition key is on. Does not look at
// engine_settings_override.
std::vector<EngineOption> engine_options(const config::GraphicsSettings& graphics);

enum class AddOutcome {
    Added,
    NoSetting,      // the file has no such Setting with options (modified or unknown file)
    AlreadyOffered, // the setting already has this option
};

struct OptionResult {
    AddOutcome outcome;
    int precedence = 0; // of the added option
};

struct EngineSettingsEdit {
    // The file parsed as well-formed XML. When false, nothing is added.
    bool well_formed = false;
    // The byte offset of the first problem when not well-formed.
    std::size_t error_at = 0;
    // The file with the options added; every byte of the player's file is
    // kept, the new elements are inserted after each setting's last option
    // in its indentation and line ending. Empty when nothing was added.
    std::string text;
    // One per option given, in order.
    std::vector<OptionResult> results;

    int added() const noexcept;
};

// Adds `options` to the engine settings XML `xml` (pure; 24 §6.1, §6.2).
EngineSettingsEdit add_engine_options(std::string_view xml, std::span<const EngineOption> options);

// Whether the path the game opens, `opened`, names its engine settings
// file `game_path` (the relative path the game holds, 24 §1.9): equal, or
// ending in it after a separator; ASCII case and the separator kind are
// ignored.
bool names_engine_settings(std::string_view opened, std::string_view game_path) noexcept;

// Builds `copy` from `player_file` with `options` added, and logs each
// addition made or skipped (24 §11.3). True when the game should read
// `copy`: the player's file was read and parsed, at least one option was
// added, and the copy is on disk. Otherwise the game reads its own file.
// Main thread, once, when the game first opens its file.
bool build_engine_settings_copy(config::ports::FileStore& files, diag::ports::Log& log,
                                const std::filesystem::path& player_file, const std::filesystem::path& copy,
                                std::span<const EngineOption> options) noexcept;

} // namespace omvr::core::render
