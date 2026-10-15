#include "core/ui/pak2.h"

#include "util/binary/align.h"
#include "util/binary/reader.h"
#include "util/binary/writer.h"

#include <cstring>

namespace omvr::core::ui {

namespace {

using util::align_up;
using util::Reader;

bool is_pak2(std::span<const std::byte> magic) noexcept {
    return magic.size() == kPak2Magic.size() &&
           std::memcmp(magic.data(), kPak2Magic.data(), kPak2Magic.size()) == 0;
}

// The NUL-terminated name at `at` in `names` (or the rest of it), moving
// `at` past its terminator.
std::string next_name(std::span<const std::byte> names, std::size_t& at) {
    const auto* chars     = reinterpret_cast<const char*>(names.data());
    const std::size_t len = ::strnlen(chars + at, names.size() - at);
    std::string name(chars + at, len);
    at += len + 1;
    return name;
}

} // namespace

std::optional<std::unordered_map<std::string, std::span<const std::byte>>> read_pak(
    std::span<const std::byte> pak) {
    Reader r(pak);
    const auto magic      = r.bytes(kPak2Magic.size());
    const auto names_size = r.get<std::uint32_t>();
    const auto count      = r.get<std::uint32_t>();
    const auto align      = r.get<std::uint32_t>();
    const auto names      = r.bytes(names_size);
    if (!r.ok() || !is_pak2(magic) || align == 0) return std::nullopt;

    std::unordered_map<std::string, std::span<const std::byte>> entries;
    std::size_t name_at = 0;
    std::size_t pos     = kPak2HeaderSize + std::size_t{names_size} + kPak2OffsetSize * std::size_t{count};
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto end          = r.get<std::uint32_t>();
        std::string name        = next_name(names, name_at);
        const std::size_t start = align_up(pos, align);
        if (!r.ok() || end < start || end > pak.size() || name_at > names.size()) return std::nullopt;
        entries.emplace(std::move(name), pak.subspan(start, end - start));
        pos = end;
    }
    return entries;
}

std::vector<std::byte> build_pak(std::uint32_t align, const std::vector<OutEntry>& entries) {
    std::string names;
    for (const auto& e : entries) names += e.name + '\0';
    std::vector<std::byte> out(kPak2HeaderSize + names.size() + kPak2OffsetSize * entries.size());
    std::memcpy(out.data(), kPak2Magic.data(), kPak2Magic.size());
    std::size_t at = kPak2Magic.size();
    for (const std::uint32_t field :
         {static_cast<std::uint32_t>(names.size()), static_cast<std::uint32_t>(entries.size()), align}) {
        util::put_u32_at(out, at, field);
        at += sizeof(field);
    }
    std::memcpy(out.data() + kPak2HeaderSize, names.data(), names.size());
    std::size_t table = kPak2HeaderSize + names.size();
    for (const auto& e : entries) {
        out.resize(align_up(out.size(), align));
        out.insert(out.end(), e.data.begin(), e.data.end());
        util::put_u32_at(out, table, static_cast<std::uint32_t>(out.size()));
        table += kPak2OffsetSize;
    }
    return out;
}

} // namespace omvr::core::ui
