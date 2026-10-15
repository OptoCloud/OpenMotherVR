// mothervr.json load / save.
//
// From: FUN_10010750 @ 0x10010750 (load) and FUN_10014680 @ 0x10014680 (save).

#include "core/config/settings_store.h"

#include "core/config/settings_json.h"
#include "core/config/settings_table.h"
#include "core/diag/logger.h"
#include "platform/win32/text.h" // R: config (A4): paths for the log without the platform

#include <atomic>
#include <cstdint>
#include <format>
#include <sstream>

namespace omvr::core::config {

namespace {

using nlohmann::json;
using platform::win32::utf8;

constexpr const char* kFilename = "mothervr.json";

std::atomic<SettingsStore*> g_process_settings{nullptr};

// The parsed file, or nullopt with `why` set to the reason it is unusable.
std::optional<json> read_document(ports::FileStore& files, const std::filesystem::path& path, std::string& why) {
    const auto text = files.read(path / kFilename);
    if (!text) {
        why = std::format("cannot open it (err=0x{:08X})", static_cast<std::uint32_t>(text.error().code));
        return std::nullopt;
    }
    try {
        std::istringstream f(*text, std::ios::binary);
        // Notepad and other Windows editors may save a UTF-8 byte-order
        // mark; players edit this file by hand, so it must still load.
        char bom[3] = {};
        f.read(bom, 3);
        if (!(f.gcount() == 3 && bom[0] == '\xEF' && bom[1] == '\xBB' && bom[2] == '\xBF')) {
            f.clear();
            f.seekg(0);
        }
        json j;
        f >> j;
        return j;
    } catch (const std::exception& e) {
        why = e.what();
        return std::nullopt;
    }
}

} // namespace

bool SettingsStore::save(const std::filesystem::path& path, const Settings& cfg) {
    const auto target = path / kFilename;
    std::uint32_t error = 0;
    bool ok = false;
    try {
        json j = g_document;
        write_config(j, cfg);
        const auto written = files_.write_atomically(target, j.dump(4));
        ok = written.has_value();
        if (!ok) error = static_cast<std::uint32_t>(written.error().code);
    } catch (const std::exception& e) {
        diag::logf("config: saving settings to {} failed ({}); the change holds for this session",
                   utf8(target), e.what());
        return false;
    }
    if (ok) {
        diag::logf("config: settings saved to {}", utf8(target));
    } else {
        diag::logf("config: saving settings to {} failed err=0x{:08X}; the change holds for this session",
                   utf8(target), error);
    }
    return ok;
}

bool SettingsStore::initialize(const std::filesystem::path& dir) {
    g_config_dir = dir;
    const auto file = dir / kFilename;
    std::string why;
    if (auto doc = read_document(files_, dir, why)) {
        current_ = read_config(*doc);
        sanitize(current_);
        g_document = std::move(*doc);
        if (!g_document.is_object()) g_document = json::object();
        g_source = "file";
        return false;
    }
    current_ = Settings{};
    g_document = json::object();
    // No file: the first launch. The caller writes the template (see
    // settings_store.h). A file that is there but does not parse is kept
    // until a setting changes. An error while checking is not "absent".
    const auto exists = files_.exists(file);
    const bool absent = exists.has_value() && !*exists;
    if (absent) {
        g_source = "defaults, first launch";
        diag::logf("config: {} absent: first launch, file follows the welcome message", utf8(file));
    } else {
        g_source = "defaults, file unusable";
        diag::logf("config: {} ignored, defaults apply: {}", utf8(file), why);
    }
    return absent;
}

void SettingsStore::save_current() {
    if (!g_config_dir.empty())
        save(g_config_dir, current());
}

void set_process_settings(SettingsStore* store) noexcept {
    g_process_settings.store(store, std::memory_order_release);
}

Settings& current() noexcept { return g_process_settings.load(std::memory_order_acquire)->current(); }

void save_current() {
    if (SettingsStore* store = g_process_settings.load(std::memory_order_acquire)) store->save_current();
}

} // namespace omvr::core::config
