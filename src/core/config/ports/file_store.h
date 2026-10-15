#pragma once

#include "util/base/error.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace omvr::core::config::ports {

// The settings file on disk. Errors carry the Windows error code.
class FileStore {
public:
    // The whole file.
    virtual util::Result<std::string> read(const std::filesystem::path& path) = 0;
    // The file written whole under a temporary name, then moved over the old
    // one in one step, so a process ended mid-write leaves the old file or
    // the new one, never a partial one.
    virtual util::Result<void> write_atomically(const std::filesystem::path& path, std::string_view text) = 0;
    // Whether the file exists; an error while checking is not "absent".
    virtual util::Result<bool> exists(const std::filesystem::path& path) = 0;

protected:
    ~FileStore() = default;
};

} // namespace omvr::core::config::ports
