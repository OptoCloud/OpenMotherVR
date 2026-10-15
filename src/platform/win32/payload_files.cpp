// Deploys the payloads embedded in the DLL's resources to the game directory.
//
// From: FUN_1001e720 @ 0x1001e720 (MOTHER.PAK) and FUN_1001e940 @
// 0x1001e940 (openvr_api.dll): the stamp-compare scheme.

#include "platform/win32/payload_files.h"

#include "core/diag/logger.h" // R: ui (A4): log through an injected Log&
#include "platform/win32/build_stamp.h"
#include "platform/win32/file_io.h"
#include "platform/win32/module.h"
#include "platform/win32/resources.h"
#include "platform/win32/text.h"

#include <windows.h>

#include <fstream>
#include <optional>
#include <span>
#include <string>

namespace omvr::platform::win32 {

namespace {

namespace diag {
using core::diag::logf;
using win32::module_directory;
using win32::utf8;
} // namespace diag

using core::ui::ports::FileIdentity;

FILETIME to_filetime(std::int64_t v) noexcept {
    return FILETIME{static_cast<DWORD>(v), static_cast<DWORD>(static_cast<std::uint64_t>(v) >> 32)};
}

bool up_to_date(const std::filesystem::path& file, const FILETIME& stamp) noexcept {
    HANDLE h = ::CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    FILETIME written{};
    const bool ok = ::GetFileTime(h, nullptr, nullptr, &written) != 0;
    ::CloseHandle(h);
    return ok && ::CompareFileTime(&written, &stamp) == 0;
}

bool write_resource(HMODULE self, int id, const std::filesystem::path& target,
                    std::int64_t build_stamp) noexcept {
    const FILETIME stamp = to_filetime(build_stamp);
    const auto bytes = resource_bytes(self, id);
    if (bytes.empty()) {
        diag::logf("payload: resource {} missing from module", id);
        return false;
    }

    HANDLE h = ::CreateFileW(target.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        diag::logf("payload: cannot create {} err=0x{:08X}", diag::utf8(target), ::GetLastError());
        return false;
    }
    DWORD written = 0;
    const bool ok = ::WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)
                    && written == bytes.size();
    const DWORD error = ok ? 0 : ::GetLastError();
    // Stamp only a complete file: the stamp is what marks it up to date.
    if (ok) ::SetFileTime(h, &stamp, &stamp, &stamp);
    ::CloseHandle(h);

    if (!ok) {
        diag::logf("payload: write to {} failed err=0x{:08X}", diag::utf8(target), error);
        ::DeleteFileW(target.c_str());
        return false;
    }
    diag::logf("payload: wrote {} ({} bytes)", diag::utf8(target), bytes.size());
    return true;
}

} // namespace

std::optional<FileIdentity> ArchiveFiles::identity(const std::filesystem::path& file) noexcept {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!::GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &a)) return std::nullopt;
    if (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return std::nullopt;
    return FileIdentity{(std::uint64_t{a.nFileSizeHigh} << 32) | a.nFileSizeLow,
                        (std::uint64_t{a.ftLastWriteTime.dwHighDateTime} << 32) | a.ftLastWriteTime.dwLowDateTime};
}

util::Result<std::vector<std::byte>> ArchiveFiles::read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return std::unexpected(util::Error{static_cast<std::int32_t>(::GetLastError()), "read"});
    std::vector<std::byte> data(static_cast<std::size_t>(in.tellg()));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        return std::unexpected(util::Error{static_cast<std::int32_t>(::GetLastError()), "read"});
    }
    return data;
}

util::Result<void> ArchiveFiles::write_atomically(const std::filesystem::path& path,
                                                  std::span<const std::byte> data) {
    unsigned long error = 0;
    bool created = false;
    if (!write_file_atomically(path, data.data(), data.size(), error, &created))
        return std::unexpected(util::Error{static_cast<std::int32_t>(error), created ? "write" : "create"});
    return {};
}

bool ArchiveFiles::stamped(const std::filesystem::path& file, core::ui::ports::FileTime time) noexcept {
    return up_to_date(file, to_filetime(time));
}

void ArchiveFiles::stamp(const std::filesystem::path& target, core::ui::ports::FileTime build_stamp) noexcept {
    HANDLE h = ::CreateFileW(target.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        const FILETIME time = to_filetime(build_stamp);
        ::SetFileTime(h, &time, &time, &time);
        ::CloseHandle(h);
    }
}

std::optional<FileIdentity> ArchiveFiles::read_sidecar(const std::filesystem::path& file) noexcept {
    HANDLE h = ::CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::nullopt;
    FileIdentity id{};
    DWORD got = 0;
    const bool ok = ::ReadFile(h, &id, sizeof(id), &got, nullptr) && got == sizeof(id);
    ::CloseHandle(h);
    return ok ? std::optional{id} : std::nullopt;
}

void ArchiveFiles::write_sidecar(const std::filesystem::path& file, const FileIdentity& id) noexcept {
    HANDLE h = ::CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    const bool ok = ::WriteFile(h, &id, sizeof(id), &written, nullptr) && written == sizeof(id);
    ::CloseHandle(h);
    if (!ok) ::DeleteFileW(file.c_str());
}

void ArchiveFiles::remove(const std::filesystem::path& path) noexcept { ::DeleteFileW(path.c_str()); }

core::ui::ports::LockState ArchiveFiles::lock(std::wstring_view name, std::chrono::milliseconds timeout) noexcept {
    return mutex_.lock(name, timeout);
}

void ArchiveFiles::unlock() noexcept { mutex_.unlock(); }

std::span<const std::byte> ArchiveFiles::payload(core::ui::ports::ResourceId id) noexcept {
    return resource_bytes(module_, id == core::ui::ports::ResourceId::OpenVrApi ? openvr_api_id_ : ui_patch_id_);
}

bool ArchiveFiles::deploy_openvr_api() noexcept {
    const auto self = static_cast<HMODULE>(module_);
    const std::int64_t build_stamp = this_module_build_filetime();
    const auto target = diag::module_directory() / "openvr_api.dll";
    if (up_to_date(target, to_filetime(build_stamp))) {
        diag::logf("payload: {} is current", diag::utf8(target));
        return true;
    }
    return write_resource(self, openvr_api_id_, target, build_stamp);
}

} // namespace omvr::platform::win32
