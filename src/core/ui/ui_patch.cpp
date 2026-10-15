#include "core/ui/ui_patch.h"

#include "core/diag/logger.h"
#include "core/ui/pak2.h"
#include "platform/win32/text.h" // R: ui (A4): paths for the log without the platform
#include "util/binary/reader.h"
#include "util/hash/crc32.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace omvr::core::ui {

namespace {

namespace diag {
using core::diag::log_line;
using core::diag::logf;
using platform::win32::utf8;
} // namespace diag

using util::crc32;
using util::Reader;

bool write_pak(ports::ArchiveFiles& files, const std::filesystem::path& path, std::uint32_t align,
               const std::vector<OutEntry>& entries) {
    const std::vector<std::byte> out = build_pak(align, entries);
    // Written whole under a temporary name, flushed, then moved over the
    // archive in one step: the game (or the other process of a Steam
    // relaunch) never opens a partly written MOTHER.PAK, and a process ended
    // mid-write leaves the old file, which is then rebuilt (spec 14 §5.8).
    auto temp = path;
    temp += L".tmp";
    const auto written = files.write_atomically(path, out);
    if (!written && written.error().context == "create") {
        diag::logf("ui patch: cannot create {} err=0x{:08X}", diag::utf8(temp),
                   static_cast<std::uint32_t>(written.error().code));
        return false;
    }
    if (!written) {
        diag::logf("ui patch: writing {} failed err=0x{:08X}", diag::utf8(path),
                   static_cast<std::uint32_t>(written.error().code));
    }
    return written.has_value();
}

} // namespace

bool build_ui_archive(ports::ArchiveFiles& files, const std::filesystem::path& stock_path,
                      std::span<const std::byte> patch, const std::filesystem::path& target) noexcept {
    try {
        const auto stock_file = files.read(stock_path);
        const auto stock = stock_file ? read_pak(*stock_file) : std::nullopt;
        if (!stock) {
            diag::logf("ui patch: cannot read {}", diag::utf8(stock_path));
            return false;
        }

        Reader r(patch);
        const auto magic = r.bytes(8);
        const auto align = r.get<std::uint32_t>();
        const auto count = r.get<std::uint32_t>();
        if (!r.ok() || std::memcmp(magic.data(), "MVRUIP01", 8) != 0 || align == 0) {
            diag::log_line("ui patch: embedded patch is malformed");
            return false;
        }

        std::vector<OutEntry> out;
        out.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto name_len = r.get<std::uint16_t>();
            const auto name_bytes = r.bytes(name_len);
            const auto kind = r.get<std::uint8_t>();
            if (!r.ok()) return false;
            OutEntry e{std::string(reinterpret_cast<const char*>(name_bytes.data()), name_len), {}};

            if (kind == 2) {
                const auto bytes = r.bytes(r.get<std::uint32_t>());
                e.data.assign(bytes.begin(), bytes.end());
            } else {
                const auto size = r.get<std::uint32_t>();
                const auto crc = r.get<std::uint32_t>();
                const auto it = stock->find(e.name);
                if (it == stock->end() || it->second.size() != size || crc32(it->second) != crc) {
                    diag::logf("ui patch: {} in {} is not the version the patch was made for",
                               e.name, diag::utf8(stock_path));
                    return false;
                }
                const auto base = it->second;
                if (kind == 0) {
                    e.data.assign(base.begin(), base.end());
                } else {
                    const auto out_size = r.get<std::uint32_t>();
                    const auto ops = r.get<std::uint32_t>();
                    e.data.reserve(out_size);
                    for (std::uint32_t k = 0; k < ops && r.ok(); ++k) {
                        if (r.get<std::uint8_t>() == 0) {
                            const auto off = r.get<std::uint32_t>();
                            const auto len = r.get<std::uint32_t>();
                            if (off > base.size() || len > base.size() - off) return false;
                            e.data.insert(e.data.end(), base.begin() + off, base.begin() + off + len);
                        } else {
                            const auto bytes = r.bytes(r.get<std::uint32_t>());
                            e.data.insert(e.data.end(), bytes.begin(), bytes.end());
                        }
                    }
                    if (e.data.size() != out_size) return false;
                }
            }
            if (!r.ok()) return false;
            out.push_back(std::move(e));
        }

        if (!write_pak(files, target, align, out)) {
            diag::logf("ui patch: cannot write {}", diag::utf8(target));
            return false;
        }
        diag::logf("ui patch: built {} ({} entries) from {}", diag::utf8(target), out.size(), diag::utf8(stock_path));
        return true;
    } catch (const std::exception& e) {
        diag::logf("ui patch: {}", e.what());
        return false;
    }
}

} // namespace omvr::core::ui
