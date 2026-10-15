#include "platform/win32/file_io.h"

#include <windows.h>

#include <fstream>
#include <iterator>
#include <system_error>

namespace omvr::platform::win32 {

util::Result<std::string> FileStore::read(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::unexpected(util::Error{static_cast<std::int32_t>(::GetLastError()), "open"});
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

util::Result<void> FileStore::write_atomically(const std::filesystem::path& path, std::string_view text) {
    unsigned long error = 0;
    if (!write_file_atomically(path, text.data(), text.size(), error))
        return std::unexpected(util::Error{static_cast<std::int32_t>(error), "write"});
    return {};
}

util::Result<bool> FileStore::exists(const std::filesystem::path& path) {
    std::error_code ec;
    const bool there = std::filesystem::exists(path, ec);
    if (ec) return std::unexpected(util::Error{ec.value(), "exists"});
    return there;
}

bool write_file_atomically(const std::filesystem::path& target, const void* data, std::size_t size,
                           unsigned long& error, bool* created) {
    auto temp = target;
    temp += L".tmp";
    HANDLE h = ::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                             nullptr);
    if (created) *created = h != INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE) {
        error = ::GetLastError();
        return false;
    }
    DWORD written = 0;
    bool ok = ::WriteFile(h, data, static_cast<DWORD>(size), &written, nullptr)
              && written == size && ::FlushFileBuffers(h);
    error = ok ? 0 : ::GetLastError();
    ::CloseHandle(h);
    if (ok && !::MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = ::GetLastError();
        ok = false;
    }
    if (!ok) ::DeleteFileW(temp.c_str());
    return ok;
}

} // namespace omvr::platform::win32
