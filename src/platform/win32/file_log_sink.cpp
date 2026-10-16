#include "platform/win32/file_log_sink.h"

#include <windows.h>

namespace omvr::platform::win32 {

void FileLogSink::open(const std::filesystem::path& file) noexcept {
    HANDLE h =
        ::CreateFileW(file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    // Append-only: each AI launch spawns multiple processes (Steam launcher,
    // then the game) and both run our DllMain. Truncating here would erase
    // the launcher's lines when the game attaches. Session boundaries are
    // marked by the "== MotherVR attach ==" header line instead.
    file_.store(h, std::memory_order_release);
}

bool FileLogSink::ready() const noexcept { return file_.load(std::memory_order_acquire) != nullptr; }

void FileLogSink::write(std::span<const char> line) noexcept {
    DWORD written = 0;
    ::WriteFile(file_.load(std::memory_order_acquire), line.data(), static_cast<DWORD>(line.size()), &written,
                nullptr);
}

// FILE_APPEND_DATA access is enough for FlushFileBuffers (the handle needs
// write or append access).
void FileLogSink::flush() noexcept {
    if (void* h = file_.load(std::memory_order_acquire)) ::FlushFileBuffers(h);
}

} // namespace omvr::platform::win32
