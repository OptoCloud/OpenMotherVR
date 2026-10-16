// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log_flush.h"
#include "core/diag/ports/log_sink.h"
#include "core/diag/thread_roles.h"

#include <atomic>
#include <span>

// The log sink with Controls.CrashSafeLog's flush policy (22 §14) between
// the logger and the file. Off (the default), it only passes each line on,
// so the log is v1's exactly. On, each line is written as before and then
// flushed by the policy of flush_policy.h. Any thread; no allocation.
namespace omvr::core::diag {

class CrashSafeSink final : public ports::LogSink {
public:
    CrashSafeSink(ports::LogSink& file, ports::LogFlush& flush, const ports::CurrentThread& thread,
                  const ThreadRoles& roles) noexcept
        : file_(file), flush_(flush), thread_(thread), roles_(roles) {}

    // Turns the policy on, from the setting read at attach. Lines written
    // before are flushed by the mod's thread once it runs.
    void enable() noexcept;
    bool enabled() const noexcept { return enabled_.load(std::memory_order_acquire); }

    bool ready() const noexcept override { return file_.ready(); }
    void write(std::span<const char> line) noexcept override;

private:
    ports::LogSink& file_;
    ports::LogFlush& flush_;
    const ports::CurrentThread& thread_;
    const ThreadRoles& roles_;
    std::atomic<bool> enabled_{false};
};

} // namespace omvr::core::diag
