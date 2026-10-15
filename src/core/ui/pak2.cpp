#include "core/ui/pak2.h"

#include "util/binary/reader.h"

#include <cstring>

namespace omvr::core::ui {

using util::Reader;

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

std::vector<std::byte> build_pak(std::uint32_t align, const std::vector<OutEntry>& entries) {
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
    return out;
}

} // namespace omvr::core::ui
