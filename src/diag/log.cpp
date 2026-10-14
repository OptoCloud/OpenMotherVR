// Append-only file logger for MotherVR.
//
// One file per game install: <dir-of-our-DLL>\mothervr.log. Each line is
// `[yyyy-MM-dd hh:mm:ss.fff pid=PPPPP] <message>\r\n`. The pid prefix matters
// because AI.exe spawns a launcher + game process and both end up running our
// DllMain; without pid in the log it's hard to tell which is which.
//
// Both processes write into the same file without coordination; see g_file
// below for how the writes stay whole.

#include "log.h"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <string>

namespace mothervr::diag {

namespace {

// Opened once and kept: opening the file per line cost about 0.1 ms, and up
// to 250 ms during a level load. FILE_APPEND_DATA keeps each WriteFile an
// atomic append, so the launcher and game processes can share the file.
HANDLE            g_file = INVALID_HANDLE_VALUE;
std::atomic<bool> g_ready{false};

int format_prefix(char* out, std::size_t cap) noexcept {
    SYSTEMTIME t{};
    ::GetLocalTime(&t);
    auto r = std::format_to_n(
        out, cap,
        "[{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} pid={:05}] ",
        static_cast<unsigned>(t.wYear), static_cast<unsigned>(t.wMonth),
        static_cast<unsigned>(t.wDay), static_cast<unsigned>(t.wHour),
        static_cast<unsigned>(t.wMinute), static_cast<unsigned>(t.wSecond),
        static_cast<unsigned>(t.wMilliseconds),
        ::GetCurrentProcessId());
    return static_cast<int>(r.out - out);
}

} // namespace

std::filesystem::path module_directory() noexcept {
    HMODULE self = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(&module_directory), &self))
        return {};
    try {
        std::wstring buf(MAX_PATH, L'\0');
        for (;;) {
            const DWORD n = ::GetModuleFileNameW(self, buf.data(), static_cast<DWORD>(buf.size()));
            if (n == 0) return {};
            if (n < buf.size()) {
                buf.resize(n);
                return std::filesystem::path{buf}.parent_path();
            }
            buf.resize(buf.size() * 2);
        }
    } catch (...) {
        return {};
    }
}

std::string utf8(const std::filesystem::path& path) noexcept {
    try {
        const std::wstring& w = path.native();
        const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                            nullptr, nullptr);
        std::string out(static_cast<std::size_t>(n), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
        return out;
    } catch (...) {
        return {};
    }
}

void init(HINSTANCE /*self*/) noexcept {
    auto dir = module_directory();
    if (dir.empty()) return;
    g_file = ::CreateFileW((dir / "mothervr.log").c_str(), FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_file == INVALID_HANDLE_VALUE) return;
    // Append-only: each AI launch spawns multiple processes (Steam launcher,
    // then the game) and both run our DllMain. Truncating here would erase
    // the launcher's lines when the game attaches. Session boundaries are
    // marked by the "== MotherVR attach ==" header line instead.
    g_ready.store(true, std::memory_order_release);
}

void log_line(std::string_view msg) noexcept {
    if (!g_ready.load(std::memory_order_acquire)) return;

    char buf[1280];
    int n = format_prefix(buf, sizeof(buf));
    if (n < 0 || n >= static_cast<int>(sizeof(buf) - 2)) return;
    const std::size_t room = sizeof(buf) - static_cast<std::size_t>(n) - 2;
    const std::size_t take = msg.size() < room ? msg.size() : room;
    std::memcpy(buf + n, msg.data(), take);
    buf[n + take]     = '\r';
    buf[n + take + 1] = '\n';

    DWORD written = 0;
    ::WriteFile(g_file, buf, static_cast<DWORD>(n + take + 2), &written, nullptr);
}

int message_box(std::string_view text, unsigned type) noexcept {
    log_line(std::string{"message box: "}.append(text));
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), n);
    return ::MessageBoxW(nullptr, wide.c_str(), L"MotherVR", type);
}

} // namespace mothervr::diag
