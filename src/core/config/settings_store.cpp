// mothervr.json load / save.
//
// From: FUN_10010750 @ 0x10010750 (load) and FUN_10014680 @ 0x10014680 (save).

#include "core/config/settings_store.h"

#include "core/config/settings_json.h"
#include "core/config/settings_table.h"
#include "core/config/tuning.h"
#include "core/diag/log_format.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <sstream>

namespace omvr::core::config {

namespace {

using diag::path_text;
using diag::ports::logf;
using nlohmann::json;

// Error context of a save that failed while building the text.
constexpr std::string_view kSerializeFailed = "serialize";

// Moves `f` past a UTF-8 byte-order mark, or leaves it at the start.
void skip_byte_order_mark(std::istream& f) {
    char bom[kUtf8ByteOrderMark.size()] = {};
    f.read(bom, sizeof(bom));
    const bool found = f.gcount() == static_cast<std::streamsize>(sizeof(bom)) &&
                       std::ranges::equal(bom, kUtf8ByteOrderMark, [](char a, unsigned char b) {
                           return static_cast<unsigned char>(a) == b;
                       });
    if (!found) {
        f.clear();
        f.seekg(0);
    }
}

} // namespace

std::expected<json, std::string> SettingsStore::read_document(const std::filesystem::path& dir) {
    const auto text = files_.read(dir / kSettingsFileName);
    if (!text) {
        return std::unexpected(
            std::format("cannot open it (err=0x{:08X})", static_cast<std::uint32_t>(text.error().code)));
    }
    try {
        std::istringstream f(*text, std::ios::binary);
        skip_byte_order_mark(f);
        json j;
        f >> j;
        return j;
    } catch (const std::exception& e) {
        return std::unexpected(std::string(e.what()));
    }
}

util::Result<void> SettingsStore::save(const std::filesystem::path& dir, const Settings& cfg) {
    const auto target = dir / kSettingsFileName;
    util::Result<void> written;
    try {
        json j = document_;
        write_config(j, cfg);
        written = files_.write_atomically(target, j.dump(kSettingsJsonIndent));
    } catch (const std::exception& e) {
        logf(log_, "config: saving settings to {} failed ({}); the change holds for this session", path_text(target),
             e.what());
        return std::unexpected(util::Error{0, kSerializeFailed});
    }
    if (written) {
        logf(log_, "config: settings saved to {}", path_text(target));
    } else {
        logf(log_, "config: saving settings to {} failed err=0x{:08X}; the change holds for this session",
             path_text(target), static_cast<std::uint32_t>(written.error().code));
    }
    return written;
}

bool SettingsStore::initialize(const std::filesystem::path& dir) {
    config_dir_ = dir;
    auto doc = read_document(dir);
    if (!doc) return use_defaults(dir / kSettingsFileName, doc.error());
    current_ = read_config(*doc);
    sanitize(current_);
    document_ = std::move(*doc);
    if (!document_.is_object()) document_ = json::object();
    source_ = "file";
    return false;
}

bool SettingsStore::use_defaults(const std::filesystem::path& file, std::string_view why) {
    current_ = Settings{};
    document_ = json::object();
    // No file: the first launch. The caller writes the template (see
    // settings_store.h). A file that is there but does not parse is kept
    // until a setting changes. An error while checking is not "absent".
    const auto exists = files_.exists(file);
    const bool absent = exists.has_value() && !*exists;
    if (absent) {
        source_ = "defaults, first launch";
        logf(log_, "config: {} absent: first launch, file follows the welcome message", path_text(file));
    } else {
        source_ = "defaults, file unusable";
        logf(log_, "config: {} ignored, defaults apply: {}", path_text(file), why);
    }
    return absent;
}

void SettingsStore::save_current() {
    // The outcome is logged by save(); a failed save leaves the settings in
    // memory, which is all a caller could do about it.
    if (config_dir_.empty()) return;
    [[maybe_unused]] const auto saved = save(config_dir_, current());
}

} // namespace omvr::core::config
