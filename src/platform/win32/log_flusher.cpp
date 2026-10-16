// OpenMotherVR-Provenance: clean
#include "platform/win32/log_flusher.h"

#include "platform/win32/process_info.h"

#include <windows.h>

#include <string_view>

namespace omvr::platform::win32 {

namespace {

constexpr std::string_view kStartedLog =
    "log: crash-safe logging on; the log is flushed to disk as it is written";
constexpr std::string_view kNoThreadLog =
    "log: crash-safe logging: the flush thread could not be started (error {}); only problems are flushed";

} // namespace

void LogFlusher::start() noexcept {
    if (started_.exchange(true)) return;
    HANDLE wake = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!wake) {
        core::diag::ports::logf(log_, kNoThreadLog, ::GetLastError());
        return;
    }
    wake_.store(wake, std::memory_order_release);
    HANDLE thread = ::CreateThread(nullptr, 0, &LogFlusher::run, this, 0, nullptr);
    if (!thread) {
        const DWORD error = ::GetLastError();
        wake_.store(nullptr, std::memory_order_release);
        ::CloseHandle(wake);
        core::diag::ports::logf(log_, kNoThreadLog, error);
        return;
    }
    ::CloseHandle(thread); // not joined (12 §7)
    log_.line(kStartedLog);
    // Lines written before the thread existed are flushed now.
    if (pending_.load(std::memory_order_acquire)) ::SetEvent(wake);
}

void LogFlusher::flush_now() noexcept { file_.flush(); }

void LogFlusher::flush_soon() noexcept {
    if (pending_.exchange(true, std::memory_order_acq_rel)) return;
    if (void* wake = wake_.load(std::memory_order_acquire)) ::SetEvent(wake);
}

unsigned long __stdcall LogFlusher::run(void* self) {
    static_cast<LogFlusher*>(self)->loop();
    return 0;
}

// The mod's thread: wait for a write, keep to one flush per interval, flush.
void LogFlusher::loop() noexcept {
    schedule_.thread_started(current_thread_id());
    for (;;) {
        if (::WaitForSingleObject(wake_.load(std::memory_order_acquire), INFINITE) != WAIT_OBJECT_0) return;
        if (const auto delay = schedule_.wait_before_flush(clock_.now()); delay.count() > 0)
            ::Sleep(static_cast<DWORD>(delay.count()));
        // Cleared before the flush: a line written during it sets the event
        // again and is flushed on the next pass.
        pending_.store(false, std::memory_order_release);
        file_.flush();
        schedule_.flushed(clock_.now());
    }
}

} // namespace omvr::platform::win32
