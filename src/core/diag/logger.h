#pragma once

#include "core/diag/ports/clock.h"
#include "core/diag/ports/log.h"
#include "core/diag/ports/log_sink.h"
#include "core/diag/tuning.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string_view>
#include <utility>

namespace omvr::core::diag {

// The project logger: each line is `[yyyy-MM-dd hh:mm:ss.fff pid=PPPPP]
// <message>\r\n`, handed to the sink in one piece. Any thread.
class Logger final : public ports::Log {
public:
    Logger(const ports::Clock& clock, ports::LogSink& sink, std::uint32_t pid) noexcept
        : clock_(clock), sink_(sink), pid_(pid) {}

    void line(std::string_view msg) noexcept override;
    void line_in(std::string_view msg, std::span<char> scratch) noexcept override;
    bool debug_enabled() const noexcept override;

    // The setting debug_enabled() reports (Settings.DebugOptions), which may
    // change while the game runs.
    void set_debug_flag(const bool* flag) noexcept { debug_flag_.store(flag, std::memory_order_release); }

private:
    const ports::Clock&      clock_;
    ports::LogSink&          sink_;
    std::uint32_t            pid_;
    std::atomic<const bool*> debug_flag_{nullptr};
};

// R: diag (A4) and every area: callers move to a Log& handed in at
// construction. Until then the loader registers its logger here at attach
// and these forward to it (no-ops before).
void set_process_log(ports::Log* log) noexcept;
ports::Log* process_log() noexcept;

// Appends one line to the process log.
void log_line(std::string_view msg) noexcept;
// log_line, rendering into `scratch` (`cap` bytes); see ports::Log::line_in.
void log_line_in(std::string_view msg, char* scratch, std::size_t cap) noexcept;

// std::format-style. Bounded to ~1 KB per line; longer messages are truncated.
template <class... Args>
inline void logf(std::format_string<Args...> fmt, Args&&... args) {
    char buf[kMessageBufferSize];
    auto r = std::format_to_n(buf, sizeof(buf) - 1, fmt, std::forward<Args>(args)...);
    log_line(std::string_view{buf, static_cast<std::size_t>(r.out - buf)});
}

} // namespace omvr::core::diag
