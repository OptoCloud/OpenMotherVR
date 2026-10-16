// OpenMotherVR-Provenance: clean
#pragma once

#include <chrono>
#include <cstdint>

namespace omvr::core::diag::ports {

// Getting the log file's written lines to disk (22 §14). Any thread.
class LogFlush {
public:
    // Flushes the file to disk now, on the calling thread. Not for the
    // render or job threads.
    virtual void flush_now() noexcept = 0;
    // Has the mod's thread flush within kLogFlushInterval. Never waits or
    // blocks: safe on the render thread.
    virtual void flush_soon() noexcept = 0;

protected:
    ~LogFlush() = default;
};

// What the mod's flush thread (22 §14) asks core logic: it is told the
// thread's id once, and before each flush how long to wait. Mod thread.
class FlushSchedule {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    // The flush thread runs; `id` is its system id (11 §1.4: `mod`).
    virtual void thread_started(std::uint32_t id) noexcept = 0;
    // Woken by a write at `now`: how long to wait before flushing.
    virtual std::chrono::milliseconds wait_before_flush(TimePoint now) noexcept = 0;
    // A flush finished at `now`.
    virtual void flushed(TimePoint now) noexcept = 0;

protected:
    ~FlushSchedule() = default;
};

// The calling thread's system id, so core logic can tell threads apart.
class CurrentThread {
public:
    virtual std::uint32_t id() const noexcept = 0;

protected:
    ~CurrentThread() = default;
};

} // namespace omvr::core::diag::ports
