#pragma once

#include "core/config/ports/file_store.h"
#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "util/base/error.h"

#include <nlohmann/json.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace omvr::core::config {

// The settings in effect and the mothervr.json they came from.
//
// The serialization library is nlohmann::json 3.4.0 (the version fingerprint
// `961c151d2e87f2686a955a9be24d316f1362bf21 3.4.0` is in .rdata).
class SettingsStore {
public:
    SettingsStore(ports::FileStore& files, diag::ports::Log& log) noexcept : files_(files), log_(log) {}

    // Write `cfg` to `dir`\mothervr.json as pretty-printed JSON (4-space
    // indent, as nlohmann's `dump(4)`) in the original's layout. Keys of the
    // file last loaded that we do not know are kept. The file is replaced in
    // one step (written under a temporary name, then moved over it), so it
    // is always either the old or the new file. Logs the outcome; on failure
    // the settings in memory still hold for this session.
    //
    // From: FUN_10014680 @ 0x10014680.
    util::Result<void> save(const std::filesystem::path& dir, const Settings& cfg);

    // The settings in effect. Reads/writes are not thread-safe: the original
    // calls into config from DllMain and from the game's UI thread only,
    // both of which run on the main thread once initialized.
    Settings& current() noexcept { return current_; }
    const Settings& current() const noexcept { return current_; }

    // One-time initialization: tries to load mothervr.json from `dir`. Without
    // a file, populates `current()` with defaults and returns true; the caller
    // then writes the template with save_current(), as the original always has
    // a mothervr.json on disk after the first launch. (DllMain does that once
    // the welcome box is closed, so a process that exits before showing it,
    // such as a Steam relaunch, leaves the first launch to the next one.) A
    // file that does not parse also gives defaults, but is left alone until a
    // setting changes.
    //
    // `dir` is typically the directory containing our DLL (= the AI install dir);
    // passing the explicit path avoids relying on the process CWD which can be
    // surprising under Steam's launcher.
    // Returns true when there was no mothervr.json, i.e. on the first launch.
    // From: FUN_10010750 @ 0x10010750 (the file-open + parse wrapper).
    bool initialize(const std::filesystem::path& dir);

    // Persist the current config to the directory passed to initialize().
    // No-op if initialize() has not been called yet. Called from the console
    // commands after any in-game mvr_* command changes a setting.
    void save_current();

    // Where the settings in effect came from at initialize(): "file",
    // "defaults, first launch" or "defaults, file unusable".
    std::string_view load_source() const noexcept { return source_; }

private:
    // The parsed file in `dir`, or why it is unusable.
    std::expected<nlohmann::json, std::string> read_document(const std::filesystem::path& dir);
    // Defaults in effect; logs why. Returns whether this is the first launch.
    bool use_defaults(const std::filesystem::path& file, std::string_view why);

    ports::FileStore& files_;
    diag::ports::Log& log_;
    Settings current_{};
    // The file as last loaded (or an empty object). save() writes into a copy
    // of it, so keys we do not know are kept.
    nlohmann::json document_ = nlohmann::json::object();
    std::filesystem::path config_dir_;
    // Where the settings in effect came from, for the settings log line.
    std::string_view source_ = "defaults";
};

} // namespace omvr::core::config
