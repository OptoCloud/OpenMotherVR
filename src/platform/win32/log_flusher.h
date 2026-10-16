// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/clock.h"
#include "core/diag/ports/log.h"
#include "core/diag/ports/log_flush.h"
#include "platform/win32/file_log_sink.h"

#include <atomic>
#include <cstdint>

// Controls.CrashSafeLog's flushing (22 §14): the log file is flushed to disk
// at once by the thread that asks (the game thread, after a problem line),
// or within a second by a thread of the mod's own (`mod`, 11 §1.4) that
// waits on an event and a timer.
namespace omvr::platform::win32 {

class LogFlusher final : public core::diag::ports::LogFlush {
public:
    LogFlusher(FileLogSink& file, const core::diag::ports::Clock& clock,
               core::diag::ports::FlushSchedule& schedule, core::diag::ports::Log& log) noexcept
        : file_(file), clock_(clock), schedule_(schedule), log_(log) {}

    // Starts the mod's flush thread. Once, after attach, outside DllMain
    // (12 §2.3). It holds no lock and is never joined: at process exit the
    // OS ends it. Without it (not started, or it could not be), deferred
    // flushes do not happen and logging is otherwise unchanged.
    void start() noexcept;

    void flush_now() noexcept override;
    void flush_soon() noexcept override;

private:
    // The thread procedure (LPTHREAD_START_ROUTINE).
    static unsigned long __stdcall run(void* self);
    void loop() noexcept;

    FileLogSink& file_;
    const core::diag::ports::Clock& clock_;
    core::diag::ports::FlushSchedule& schedule_;
    core::diag::ports::Log& log_;
    // The auto-reset event that wakes the thread (a HANDLE), published once
    // the thread exists; never closed.
    std::atomic<void*> wake_{nullptr};
    // A write since the thread last flushed. Only its rise from false sets
    // the event, so a burst of lines costs one SetEvent.
    std::atomic<bool> pending_{false};
    std::atomic<bool> started_{false};
};

} // namespace omvr::platform::win32
