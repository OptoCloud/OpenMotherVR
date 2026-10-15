#include "platform/win32/file_io.h"

#include "core/config/tuning.h"
#include "platform/win32/unique_handle.h"

#include <windows.h>

#include <fstream>
#include <iterator>
#include <system_error>

namespace omvr::platform::win32 {

namespace {

// Error contexts the callers tell apart (ArchiveFiles::write_atomically).
constexpr std::string_view kCreateFailed = "create";
constexpr std::string_view kWriteFailed  = "write";

util::Error last_error(std::string_view context) noexcept {
    return {static_cast<std::int32_t>(::GetLastError()), context};
}

// Writes all of `data` to `file` and flushes it to disk.
bool write_whole(const UniqueHandle& file, std::span<const std::byte> data) noexcept {
    DWORD written = 0;
    return ::WriteFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
           written == data.size() && ::FlushFileBuffers(file.get());
}

} // namespace

util::Result<std::string> FileStore::read(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::unexpected(last_error("open"));
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

util::Result<void> FileStore::write_atomically(const std::filesystem::path& path, std::string_view text) {
    return write_file_atomically(path, std::as_bytes(std::span{text.data(), text.size()}));
}

util::Result<bool> FileStore::exists(const std::filesystem::path& path) {
    std::error_code ec;
    const bool there = std::filesystem::exists(path, ec);
    if (ec) return std::unexpected(util::Error{ec.value(), "exists"});
    return there;
}

std::filesystem::path temporary_path_for(const std::filesystem::path& target) {
    auto temp = target;
    temp += core::config::kTemporaryFileSuffix;
    return temp;
}

util::Result<void> write_file_atomically(const std::filesystem::path& target,
                                         std::span<const std::byte> data) noexcept {
    try {
        const auto temp = temporary_path_for(target);
        util::Result<void> result;
        {
            const UniqueHandle file{::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                                  FILE_ATTRIBUTE_NORMAL, nullptr)};
            if (!file) return std::unexpected(last_error(kCreateFailed));
            // The error is captured before the handle is closed.
            if (!write_whole(file, data)) result = std::unexpected(last_error(kWriteFailed));
        }
        if (result &&
            !::MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            result = std::unexpected(last_error(kWriteFailed));
        if (!result) ::DeleteFileW(temp.c_str());
        return result;
    } catch (...) {
        return std::unexpected(util::Error{ERROR_OUTOFMEMORY, kCreateFailed});
    }
}

} // namespace omvr::platform::win32
