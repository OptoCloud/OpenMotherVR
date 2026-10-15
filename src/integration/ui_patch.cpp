#include "ui_patch.h"

#include "diag/log.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mothervr::integration {

namespace {

// CRC-32 as zlib computes it (reflected, polynomial 0xEDB88320).
constexpr std::array<std::uint32_t, 256> kCrcTable = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        t[i] = c;
    }
    return t;
}();

std::uint32_t crc32(std::span<const std::byte> data) noexcept {
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::byte b : data) c = kCrcTable[(c ^ std::to_integer<std::uint32_t>(b)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// Bounds-checked little-endian reader over a byte span.
class Reader {
public:
    explicit Reader(std::span<const std::byte> data) : data_(data) {}
    bool ok() const noexcept { return ok_; }
    template <class T>
    T get() noexcept {
        T v{};
        if (!take(sizeof(T))) return v;
        std::memcpy(&v, data_.data() + pos_ - sizeof(T), sizeof(T));
        return v;
    }
    std::span<const std::byte> bytes(std::size_t n) noexcept {
        if (!take(n)) return {};
        return data_.subspan(pos_ - n, n);
    }

private:
    bool take(std::size_t n) noexcept {
        if (!ok_ || data_.size() - pos_ < n) return ok_ = false;
        pos_ += n;
        return true;
    }
    std::span<const std::byte> data_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

std::optional<std::vector<std::byte>> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return std::nullopt;
    std::vector<std::byte> data(static_cast<std::size_t>(in.tellg()));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        return std::nullopt;
    }
    return data;
}

// PAK2: "PAK2", u32 names size, u32 count, u32 alignment, NUL-terminated
// names, u32 end offset per entry, data; each entry starts at the previous
// end (or the end of the table) rounded up to the alignment.
std::optional<std::unordered_map<std::string, std::span<const std::byte>>> read_pak(
    std::span<const std::byte> pak) {
    Reader r(pak);
    const auto magic = r.bytes(4);
    const auto names_size = r.get<std::uint32_t>();
    const auto count = r.get<std::uint32_t>();
    const auto align = r.get<std::uint32_t>();
    const auto names = r.bytes(names_size);
    if (!r.ok() || std::memcmp(magic.data(), "PAK2", 4) != 0 || align == 0) return std::nullopt;

    std::unordered_map<std::string, std::span<const std::byte>> entries;
    std::size_t name_at = 0;
    std::size_t pos = 16 + std::size_t(names_size) + 4 * std::size_t(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto end = r.get<std::uint32_t>();
        const auto* chars = reinterpret_cast<const char*>(names.data());
        const std::size_t len = ::strnlen(chars + name_at, names.size() - name_at);
        std::string name(chars + name_at, len);
        name_at += len + 1;
        const std::size_t start = (pos + align - 1) / align * align;
        if (!r.ok() || end < start || end > pak.size() || name_at > names.size()) return std::nullopt;
        entries.emplace(std::move(name), pak.subspan(start, end - start));
        pos = end;
    }
    return entries;
}

struct OutEntry {
    std::string name;
    std::vector<std::byte> data;
};

bool write_pak(const std::filesystem::path& path, std::uint32_t align, const std::vector<OutEntry>& entries) {
    std::string names;
    for (const auto& e : entries) names += e.name + '\0';
    std::vector<std::byte> out(16 + names.size() + 4 * entries.size());
    const std::uint32_t header[4] = {0x324B4150u /* "PAK2" */, static_cast<std::uint32_t>(names.size()),
                                     static_cast<std::uint32_t>(entries.size()), align};
    std::memcpy(out.data(), header, sizeof(header));
    std::memcpy(out.data() + 16, names.data(), names.size());
    std::size_t table = 16 + names.size();
    for (const auto& e : entries) {
        const std::size_t start = (out.size() + align - 1) / align * align;
        out.resize(start);
        out.insert(out.end(), e.data.begin(), e.data.end());
        const auto end = static_cast<std::uint32_t>(out.size());
        std::memcpy(out.data() + table, &end, 4);
        table += 4;
    }
    // Written whole under a temporary name, flushed, then moved over the
    // archive in one step: the game (or the other process of a Steam
    // relaunch) never opens a partly written MOTHER.PAK, and a process ended
    // mid-write leaves the old file, which is then rebuilt (spec 14 §5.8).
    auto temp = path;
    temp += L".tmp";
    HANDLE h = ::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        diag::logf("ui patch: cannot create {} err=0x{:08X}", diag::utf8(temp), ::GetLastError());
        return false;
    }
    DWORD written = 0;
    bool ok = ::WriteFile(h, out.data(), static_cast<DWORD>(out.size()), &written, nullptr)
              && written == out.size() && ::FlushFileBuffers(h);
    DWORD error = ok ? 0 : ::GetLastError();
    ::CloseHandle(h);
    if (ok && !::MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = ::GetLastError();
        ok = false;
    }
    if (!ok) {
        diag::logf("ui patch: writing {} failed err=0x{:08X}", diag::utf8(path), error);
        ::DeleteFileW(temp.c_str());
    }
    return ok;
}

} // namespace

bool build_ui_archive(const std::filesystem::path& stock_path, std::span<const std::byte> patch,
                      const std::filesystem::path& target) noexcept {
    try {
        const auto stock_file = read_file(stock_path);
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

        if (!write_pak(target, align, out)) {
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

} // namespace mothervr::integration
