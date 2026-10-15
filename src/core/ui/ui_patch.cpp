#include "core/ui/ui_patch.h"

#include "core/config/tuning.h"
#include "core/diag/log_format.h"
#include "core/ui/pak2.h"
#include "util/binary/reader.h"
#include "util/hash/crc32.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace omvr::core::ui {

namespace {

using diag::path_text;
using diag::ports::logf;
using util::crc32;
using util::Reader;

using StockEntries = std::unordered_map<std::string, std::span<const std::byte>>;

// The error context ArchiveFiles::write_atomically gives when the temporary
// file could not be created.
constexpr std::string_view kCreateFailed = "create";

bool write_pak(ports::ArchiveFiles& files, diag::ports::Log& log, const std::filesystem::path& path,
               std::uint32_t align, const std::vector<OutEntry>& entries) {
    const std::vector<std::byte> out = build_pak(align, entries);
    // Written whole under a temporary name, flushed, then moved over the
    // archive in one step: the game (or the other process of a Steam
    // relaunch) never opens a partly written MOTHER.PAK, and a process ended
    // mid-write leaves the old file, which is then rebuilt (spec 14 §5.8).
    auto temp = path;
    temp += config::kTemporaryFileSuffix;
    const auto written = files.write_atomically(path, out);
    if (!written && written.error().context == kCreateFailed) {
        logf(log, "ui patch: cannot create {} err=0x{:08X}", path_text(temp),
             static_cast<std::uint32_t>(written.error().code));
        return false;
    }
    if (!written) {
        logf(log, "ui patch: writing {} failed err=0x{:08X}", path_text(path),
             static_cast<std::uint32_t>(written.error().code));
    }
    return written.has_value();
}

// The stock entry `name` of `stock`, checked against the size and CRC-32
// the patch was made from; nullopt when it is missing or another version.
std::optional<std::span<const std::byte>> checked_stock(const StockEntries& stock, const std::string& name,
                                                        std::uint32_t size, std::uint32_t crc) {
    const auto it = stock.find(name);
    if (it == stock.end() || it->second.size() != size || crc32(it->second) != crc) return std::nullopt;
    return it->second;
}

// Runs a Delta entry's operations on `base` into `out`. False when an
// operation reaches outside `base` or the result is not `out_size` long.
bool apply_delta(Reader& r, std::span<const std::byte> base, std::vector<std::byte>& out) {
    const auto out_size = r.get<std::uint32_t>();
    const auto ops      = r.get<std::uint32_t>();
    out.reserve(out_size);
    for (std::uint32_t k = 0; k < ops && r.ok(); ++k) {
        if (static_cast<DeltaOp>(r.get<std::uint8_t>()) == DeltaOp::Copy) {
            const auto off = r.get<std::uint32_t>();
            const auto len = r.get<std::uint32_t>();
            if (off > base.size() || len > base.size() - off) return false;
            out.insert(out.end(), base.begin() + off, base.begin() + off + len);
        } else {
            // Any other operation byte inserts, as scripts/ui_patch.py reads it.
            const auto bytes = r.bytes(r.get<std::uint32_t>());
            out.insert(out.end(), bytes.begin(), bytes.end());
        }
    }
    return out.size() == out_size;
}

// Outcome of reading one patch entry.
enum class EntryResult { Ok, Malformed, WrongStock };

// Reads the entry after its name into `e`, from `stock` where it says so.
EntryResult read_entry_data(Reader& r, PatchEntry kind, const StockEntries& stock, OutEntry& e) {
    if (kind == PatchEntry::New) {
        const auto bytes = r.bytes(r.get<std::uint32_t>());
        e.data.assign(bytes.begin(), bytes.end());
        return EntryResult::Ok;
    }
    const auto size = r.get<std::uint32_t>();
    const auto crc  = r.get<std::uint32_t>();
    const auto base = checked_stock(stock, e.name, size, crc);
    if (!base) return EntryResult::WrongStock;
    if (kind == PatchEntry::Stock) {
        e.data.assign(base->begin(), base->end());
        return EntryResult::Ok;
    }
    // Any other kind is a delta, as scripts/ui_patch.py writes them.
    return apply_delta(r, *base, e.data) ? EntryResult::Ok : EntryResult::Malformed;
}

// The output archive's entries, or nullopt (logged where the reason is
// one a player can act on).
std::optional<std::vector<OutEntry>> read_entries(Reader& r, std::uint32_t count, const StockEntries& stock,
                                                  diag::ports::Log& log,
                                                  const std::filesystem::path& stock_path) {
    std::vector<OutEntry> out;
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto name_len   = r.get<std::uint16_t>();
        const auto name_bytes = r.bytes(name_len);
        const auto kind       = static_cast<PatchEntry>(r.get<std::uint8_t>());
        if (!r.ok()) return std::nullopt;
        OutEntry e{std::string(reinterpret_cast<const char*>(name_bytes.data()), name_len), {}};
        const EntryResult result = read_entry_data(r, kind, stock, e);
        if (result == EntryResult::WrongStock) {
            logf(log, "ui patch: {} in {} is not the version the patch was made for", e.name,
                 path_text(stock_path));
            return std::nullopt;
        }
        if (result != EntryResult::Ok || !r.ok()) return std::nullopt;
        out.push_back(std::move(e));
    }
    return out;
}

bool is_patch(std::span<const std::byte> magic) noexcept {
    return magic.size() == kUiPatchMagic.size() &&
           std::memcmp(magic.data(), kUiPatchMagic.data(), kUiPatchMagic.size()) == 0;
}

} // namespace

bool build_ui_archive(ports::ArchiveFiles& files, diag::ports::Log& log,
                      const std::filesystem::path& stock_path, std::span<const std::byte> patch,
                      const std::filesystem::path& target) noexcept {
    try {
        const auto stock_file = files.read(stock_path);
        const auto stock      = stock_file ? read_pak(*stock_file) : std::nullopt;
        if (!stock) {
            logf(log, "ui patch: cannot read {}", path_text(stock_path));
            return false;
        }

        Reader r(patch);
        const auto magic = r.bytes(kUiPatchMagic.size());
        const auto align = r.get<std::uint32_t>();
        const auto count = r.get<std::uint32_t>();
        if (!r.ok() || !is_patch(magic) || align == 0) {
            log.line("ui patch: embedded patch is malformed");
            return false;
        }

        const auto out = read_entries(r, count, *stock, log, stock_path);
        if (!out) return false;
        if (!write_pak(files, log, target, align, *out)) {
            logf(log, "ui patch: cannot write {}", path_text(target));
            return false;
        }
        logf(log, "ui patch: built {} ({} entries) from {}", path_text(target), out->size(),
             path_text(stock_path));
        return true;
    } catch (const std::exception& e) {
        logf(log, "ui patch: {}", e.what());
        return false;
    }
}

} // namespace omvr::core::ui
