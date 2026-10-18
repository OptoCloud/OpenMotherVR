// Deploys the payloads embedded in the DLL's resources to the game directory.
//
// From: FUN_1001e720 @ 0x1001e720 (MOTHER.PAK) and FUN_1001e940 @
// 0x1001e940 (openvr_api.dll): the stamp-compare scheme.

#include "platform/win32/payload_files.h"

#include "platform/win32/build_stamp.h"
#include "platform/win32/constants.h"
#include "platform/win32/file_io.h"
#include "platform/win32/last_error.h"
#include "platform/win32/module.h"
#include "platform/win32/resources.h"
#include "platform/win32/text.h"
#include "platform/win32/unique_handle.h"

#include <windows.h>

#include <fstream>
#include <optional>
#include <span>
#include <string>

namespace omvr::platform::win32 {

namespace {

using core::diag::ports::logf;
using core::ui::ports::FileIdentity;
using core::ui::ports::FileTime;

FILETIME to_filetime(FileTime v) noexcept {
    const auto bits = static_cast<std::uint64_t>(v);
    return FILETIME{static_cast<DWORD>(bits), static_cast<DWORD>(bits >> kDwordBits)};
}

std::uint64_t join(DWORD high, DWORD low) noexcept { return (std::uint64_t{high} << kDwordBits) | low; }

UniqueHandle open_existing(const std::filesystem::path& file, DWORD access, DWORD share) noexcept {
    return UniqueHandle{
        ::CreateFileW(file.c_str(), access, share, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
}

UniqueHandle create_always(const std::filesystem::path& file, DWORD access) noexcept {
    return UniqueHandle{
        ::CreateFileW(file.c_str(), access, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
}

bool up_to_date(const std::filesystem::path& file, const FILETIME& stamp) noexcept {
    const UniqueHandle h = open_existing(file, GENERIC_READ, FILE_SHARE_READ);
    if (!h) return false;
    FILETIME written{};
    return ::GetFileTime(h.get(), nullptr, nullptr, &written) != 0 &&
           ::CompareFileTime(&written, &stamp) == 0;
}

} // namespace

bool ArchiveFiles::write_resource(int id, const std::filesystem::path& target,
                                  FileTime build_stamp) noexcept {
    const FILETIME stamp = to_filetime(build_stamp);
    const auto bytes     = resource_bytes(module_, id);
    if (bytes.empty()) {
        logf(log_, "payload: resource {} missing from module", id);
        return false;
    }

    bool ok     = false;
    DWORD error = 0;
    {
        const UniqueHandle h = create_always(target, GENERIC_READ | GENERIC_WRITE);
        if (!h) {
            logf(log_, "payload: cannot create {} err=0x{:08X}", utf8(target), ::GetLastError());
            return false;
        }
        ok = write_all(h, bytes, false);
        // Captured before the handle is closed.
        error = ok ? 0 : ::GetLastError();
        // Stamp only a complete file: the stamp is what marks it up to date.
        if (ok) ::SetFileTime(h.get(), &stamp, &stamp, &stamp);
    }
    if (!ok) {
        logf(log_, "payload: write to {} failed err=0x{:08X}", utf8(target), error);
        ::DeleteFileW(target.c_str());
        return false;
    }
    logf(log_, "payload: wrote {} ({} bytes)", utf8(target), bytes.size());
    return true;
}

std::optional<FileIdentity> ArchiveFiles::identity(const std::filesystem::path& file) noexcept {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!::GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &a)) return std::nullopt;
    if (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return std::nullopt;
    return FileIdentity{join(a.nFileSizeHigh, a.nFileSizeLow),
                        join(a.ftLastWriteTime.dwHighDateTime, a.ftLastWriteTime.dwLowDateTime)};
}

util::Result<std::vector<std::byte>> ArchiveFiles::read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return std::unexpected(last_error("read"));
    std::vector<std::byte> data(static_cast<std::size_t>(in.tellg()));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        return std::unexpected(last_error("read"));
    }
    return data;
}

util::Result<void> ArchiveFiles::write_atomically(const std::filesystem::path& path,
                                                  std::span<const std::byte> data) {
    return write_file_atomically(path, data);
}

bool ArchiveFiles::stamped(const std::filesystem::path& file, FileTime time) noexcept {
    return up_to_date(file, to_filetime(time));
}

void ArchiveFiles::stamp(const std::filesystem::path& target, FileTime build_stamp) noexcept {
    if (const UniqueHandle h = open_existing(target, FILE_WRITE_ATTRIBUTES, 0)) {
        const FILETIME time = to_filetime(build_stamp);
        ::SetFileTime(h.get(), &time, &time, &time);
    }
}

// The sidecar holds the FileIdentity's bytes as they are (16 bytes: size,
// then last-write time, little-endian).
std::optional<FileIdentity> ArchiveFiles::read_sidecar(const std::filesystem::path& file) noexcept {
    const UniqueHandle h = open_existing(file, GENERIC_READ, FILE_SHARE_READ);
    if (!h) return std::nullopt;
    FileIdentity id{};
    DWORD got     = 0;
    const bool ok = ::ReadFile(h.get(), &id, sizeof(id), &got, nullptr) && got == sizeof(id);
    return ok ? std::optional{id} : std::nullopt;
}

void ArchiveFiles::write_sidecar(const std::filesystem::path& file, const FileIdentity& id) noexcept {
    bool ok = false;
    {
        const UniqueHandle h = create_always(file, GENERIC_WRITE);
        if (!h) return;
        ok = write_all(h, std::as_bytes(std::span{&id, 1}), false);
    }
    if (!ok) ::DeleteFileW(file.c_str());
}

void ArchiveFiles::remove(const std::filesystem::path& path) noexcept { ::DeleteFileW(path.c_str()); }

core::ui::ports::LockState ArchiveFiles::lock(std::wstring_view name,
                                              std::chrono::milliseconds timeout) noexcept {
    return mutex_.lock(name, timeout);
}

void ArchiveFiles::unlock() noexcept { mutex_.unlock(); }

std::span<const std::byte> ArchiveFiles::payload(core::ui::ports::ResourceId id) noexcept {
    return resource_bytes(module_,
                          id == core::ui::ports::ResourceId::OpenVrApi ? openvr_api_id_ : ui_patch_id_);
}

bool ArchiveFiles::deploy_openvr_api() noexcept {
    const FileTime build_stamp = this_module_build_filetime();
    const auto target          = module_directory() / kOpenVrApiFileName;
    if (up_to_date(target, to_filetime(build_stamp))) {
        logf(log_, "payload: {} is current", utf8(target));
        return true;
    }
    return write_resource(openvr_api_id_, target, build_stamp);
}

} // namespace omvr::platform::win32
