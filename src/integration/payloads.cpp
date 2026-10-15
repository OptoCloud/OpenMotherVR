// Deploys the payloads embedded in the DLL's resources to the game directory.
//
// The original writes through a file mapping and memcpy; a plain WriteFile is
// equivalent. Both compare the target's last-write time against the mod's own
// PE build stamp and skip the write when they match; MOTHER.PAK must also
// still match the UI.PAK it was built from (a sidecar file records which).

#include "payloads.h"

#include "diag/log.h"
#include "dialogs.h"
#include "exe_check.h"
#include "resource.h"
#include "ui_patch.h"

#include <optional>
#include <span>
#include <string>

namespace mothervr::integration {

namespace {

FILETIME to_filetime(std::int64_t v) noexcept {
    return FILETIME{static_cast<DWORD>(v), static_cast<DWORD>(static_cast<std::uint64_t>(v) >> 32)};
}

std::span<const std::byte> resource_bytes(HMODULE self, int id) noexcept {
    HRSRC info = ::FindResourceW(self, MAKEINTRESOURCEW(id), L"BINARY");
    if (!info) return {};
    HGLOBAL data = ::LoadResource(self, info);
    if (!data) return {};
    return {static_cast<const std::byte*>(::LockResource(data)), ::SizeofResource(self, info)};
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

// What MOTHER.PAK was built from: UI.PAK's size and last-write time.
struct FileIdentity {
    std::uint64_t size = 0;
    std::uint64_t written = 0;
    bool operator==(const FileIdentity&) const = default;
};

std::optional<FileIdentity> file_identity(const std::filesystem::path& file) noexcept {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!::GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &a)) return std::nullopt;
    if (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return std::nullopt;
    return FileIdentity{(std::uint64_t{a.nFileSizeHigh} << 32) | a.nFileSizeLow,
                        (std::uint64_t{a.ftLastWriteTime.dwHighDateTime} << 32) | a.ftLastWriteTime.dwLowDateTime};
}

// MOTHER.PAK.src holds the FileIdentity of the UI.PAK it was built from.
std::optional<FileIdentity> read_sidecar(const std::filesystem::path& file) noexcept {
    HANDLE h = ::CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::nullopt;
    FileIdentity id{};
    DWORD got = 0;
    const bool ok = ::ReadFile(h, &id, sizeof(id), &got, nullptr) && got == sizeof(id);
    ::CloseHandle(h);
    return ok ? std::optional{id} : std::nullopt;
}

void write_sidecar(const std::filesystem::path& file, const FileIdentity& id) noexcept {
    HANDLE h = ::CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    const bool ok = ::WriteFile(h, &id, sizeof(id), &written, nullptr) && written == sizeof(id);
    ::CloseHandle(h);
    if (!ok) ::DeleteFileW(file.c_str());
}

// Reuses MOTHER.PAK if it is current, else builds it (unless `may_build` is
// false: another process holds the build lock).
bool reuse_or_build(HMODULE self, const std::filesystem::path& stock, const std::filesystem::path& target,
                    const std::filesystem::path& sidecar, const FileIdentity& source, std::int64_t build_stamp,
                    bool may_build) noexcept {
    if (up_to_date(target, to_filetime(build_stamp)) && read_sidecar(sidecar) == source) {
        diag::logf("payload: {} is current", diag::utf8(target));
        return true;
    }
    if (!may_build) {
        diag::logf("payload: {} is being built by another process and is not ready after 5 s; "
                   "options page off this session, VR not affected", diag::utf8(target));
        return false;
    }

    // Built from the player's own UI.PAK and the embedded patch; no game
    // files ship with the mod.
    ::DeleteFileW(sidecar.c_str());
    if (!build_ui_archive(stock, resource_bytes(self, IDR_UI_PATCH), target)) return false;
    HANDLE h = ::CreateFileW(target.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        const FILETIME stamp = to_filetime(build_stamp);
        ::SetFileTime(h, &stamp, &stamp, &stamp);
        ::CloseHandle(h);
    }
    write_sidecar(sidecar, source);
    return true;
}

} // namespace

bool deploy_mother_pak(HMODULE self, const std::filesystem::path& game_dir,
                       std::int64_t build_stamp) noexcept {
    const auto target = game_dir / "DATA" / "MOTHER.PAK";
    const auto stock = game_dir / "DATA" / "UI.PAK";
    auto sidecar = target;
    sidecar += ".src";

    // MOTHER.PAK is built from UI.PAK, so a game update or a language switch
    // that replaces UI.PAK makes it stale even when the DLL is unchanged.
    const auto source = file_identity(stock);
    if (!source) {
        diag::logf("payload: {} not found", diag::utf8(stock));
        // The original disables the mod here; we only lose the options page.
        post_message_box("Failed to find UI.PAK!\n[" + diag::utf8(stock) +
                             "]\n\nThe MotherVR options page will be unavailable for this session.",
                         MB_OK | MB_ICONERROR, nullptr, "UI.PAK missing");
        return false;
    }
    // When Steam relaunches the game, two processes attach close together
    // and both come here: one builds, the other waits for it and then
    // reuses its archive, instead of both writing the same files. 5 s is
    // well above a rebuild and short enough that a stuck process does not
    // hold up the game's start noticeably (spec 14 §5.8).
    HANDLE mutex = ::CreateMutexW(nullptr, FALSE, L"Local\\MotherVR.MotherPak");
    bool held = false;
    if (mutex) {
        const DWORD w = ::WaitForSingleObject(mutex, 5000);
        held = w == WAIT_OBJECT_0 || w == WAIT_ABANDONED;
    }
    // Without a mutex at all (cannot be created) build as before.
    const bool may_build = held || !mutex;
    const bool ok = reuse_or_build(self, stock, target, sidecar, *source, build_stamp, may_build);
    if (held) ::ReleaseMutex(mutex);
    if (mutex) ::CloseHandle(mutex);
    return ok;
}

bool deploy_openvr_api() noexcept {
    HMODULE self = nullptr;
    ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&deploy_openvr_api), &self);
    const std::int64_t build_stamp = this_module_build_filetime();
    const auto target = diag::module_directory() / "openvr_api.dll";
    if (up_to_date(target, to_filetime(build_stamp))) {
        diag::logf("payload: {} is current", diag::utf8(target));
        return true;
    }
    return write_resource(self, IDR_OPENVR_API, target, build_stamp);
}

} // namespace mothervr::integration
