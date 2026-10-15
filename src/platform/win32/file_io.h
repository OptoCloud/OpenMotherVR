#pragma once

#include "core/config/ports/file_store.h"
#include "util/base/error.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace omvr::platform::win32 {

// Files through the Windows API; errors carry GetLastError() captured at the
// failing call.
class FileStore final : public core::config::ports::FileStore {
public:
    util::Result<std::string> read(const std::filesystem::path& path) override;
    util::Result<void> write_atomically(const std::filesystem::path& path, std::string_view text) override;
    util::Result<bool> exists(const std::filesystem::path& path) override;
};

// The one atomic file writer (mothervr.json and MOTHER.PAK): the file is
// written whole under a temporary name (`target` + core::config::kTemporaryFileSuffix),
// flushed, then moved over the old one in one step, so a process ended
// mid-write (a crash, a Steam relaunch) leaves the old file or the new one,
// never a partial one. A leftover temporary file is never read and is
// replaced by the next write. The error's context is "create" when the
// temporary file could not be created, "write" for a later failure.
util::Result<void> write_file_atomically(const std::filesystem::path& target,
                                         std::span<const std::byte> data) noexcept;

// The temporary file write_file_atomically writes `target` through.
std::filesystem::path temporary_path_for(const std::filesystem::path& target);

} // namespace omvr::platform::win32
