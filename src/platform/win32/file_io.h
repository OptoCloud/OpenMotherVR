#pragma once

#include "core/config/ports/file_store.h"

#include <filesystem>
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

// The file written whole under a temporary name (`target` + ".tmp"),
// flushed, then moved over the old one in one step, so a process ended
// mid-write (a crash, a Steam relaunch) leaves the old file or the new one,
// never a partial one. A leftover temporary file is never read and is
// replaced by the next write. False with `error` set on failure;
// `created`, if given, says whether the temporary file could be created.
bool write_file_atomically(const std::filesystem::path& target, const void* data, std::size_t size,
                           unsigned long& error, bool* created = nullptr);

} // namespace omvr::platform::win32
