#pragma once

#include "core/config/ports/file_store.h"
#include "core/config/settings.h"

#include <nlohmann/json.hpp>

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
    explicit SettingsStore(ports::FileStore& files) noexcept : files_(files) {}

    // Write `cfg` to `path` as pretty-printed JSON (4-space indent, as
    // nlohmann's `dump(4)`) in the original's layout. Keys of the file last
    // loaded that we do not know are kept. The file is replaced in one step
    // (written under a temporary name, then moved over it), so it is always
    // either the old or the new file. Logs the outcome; false if it failed, in
    // which case the settings in memory still hold for this session.
    //
    // From: FUN_10014680 @ 0x10014680.
    bool save(const std::filesystem::path& path, const Settings& cfg);

    // The settings in effect. Reads/writes are not thread-safe: the original
    // calls into config from DllMain and from the game's UI thread only,
    // both of which run on the main thread once initialized.
    Settings& current() noexcept { return current_; }

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
    std::string_view load_source() const noexcept { return g_source; }

private:
    ports::FileStore& files_;
    Settings          current_{};
    // The file as last loaded (or an empty object). save() writes into a copy
    // of it, so keys we do not know are kept.
    nlohmann::json        g_document = nlohmann::json::object();
    std::filesystem::path g_config_dir;
    // Where the settings in effect came from, for the settings log line.
    std::string_view      g_source = "defaults";
};

// R: config (A4) and every area: callers take a const Settings& at
// construction. Until then the loader registers its store here at attach.
void set_process_settings(SettingsStore* store) noexcept;
Settings& current() noexcept;
void save_current();

} // namespace omvr::core::config
