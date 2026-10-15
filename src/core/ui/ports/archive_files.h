#pragma once

#include "util/base/error.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace omvr::core::ui::ports {

// What MOTHER.PAK was built from: UI.PAK's size and last-write time.
struct FileIdentity {
    std::uint64_t size                         = 0;
    std::uint64_t written                      = 0;
    bool operator==(const FileIdentity&) const = default;
};

// A FILETIME: 100-ns intervals since 1601-01-01 UTC.
using FileTime = std::int64_t;

// The payloads embedded in the DLL.
enum class ResourceId { OpenVrApi, UiPatch };

// The outcome of taking the build lock: whether it is held, and whether a
// lock exists at all (without one, a build goes ahead as before).
struct LockState {
    bool held   = false;
    bool exists = false;
};

// The files the UI archive is built from and into. Game thread at attach.
class ArchiveFiles {
public:
    // Size and last-write time, or nullopt when absent or a directory.
    virtual std::optional<FileIdentity> identity(const std::filesystem::path& path) noexcept = 0;
    virtual util::Result<std::vector<std::byte>> read(const std::filesystem::path& path)     = 0;
    // Error context "create" when the temporary file could not be created,
    // "write" for a later failure.
    virtual util::Result<void> write_atomically(const std::filesystem::path& path,
                                                std::span<const std::byte> data) = 0;
    // The file's last-write time equals `stamp`.
    virtual bool stamped(const std::filesystem::path& path, FileTime stamp) noexcept = 0;
    // Sets all three file times to `stamp`.
    virtual void stamp(const std::filesystem::path& path, FileTime stamp) noexcept                 = 0;
    virtual std::optional<FileIdentity> read_sidecar(const std::filesystem::path& path) noexcept   = 0;
    virtual void write_sidecar(const std::filesystem::path& path, const FileIdentity& id) noexcept = 0;
    virtual void remove(const std::filesystem::path& path) noexcept                                = 0;
    // A lock shared between the processes of one game launch.
    virtual LockState lock(std::wstring_view name, std::chrono::milliseconds timeout) noexcept = 0;
    virtual void unlock() noexcept                                                             = 0;
    virtual std::span<const std::byte> payload(ResourceId id) noexcept                         = 0;

protected:
    ~ArchiveFiles() = default;
};

} // namespace omvr::core::ui::ports
