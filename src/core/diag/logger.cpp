// Append-only logger.
//
// One file per game install: <dir-of-our-DLL>\mothervr.log. Each line is
// `[yyyy-MM-dd hh:mm:ss.fff pid=PPPPP] <message>\r\n`. The pid prefix matters
// because AI.exe spawns a launcher + game process and both end up running our
// DllMain; without pid in the log it's hard to tell which is which.
//
// Both processes write into the same file without coordination; the sink
// (platform::win32::FileLogSink) keeps each write whole.

#include "core/diag/logger.h"

#include "core/diag/log_format.h"

#include <cstring>

namespace omvr::core::diag {

namespace {

std::atomic<ports::Log*> g_process_log{nullptr};

} // namespace

void Logger::line_in(std::string_view msg, std::span<char> scratch) noexcept {
    char* buf = scratch.data();
    const std::size_t cap = scratch.size();
    if (!sink_.ready() || !buf || cap < 3) return;

    int n = format_prefix(buf, cap, clock_.local_time(), pid_);
    if (n < 0 || n >= static_cast<int>(cap - 2)) return;
    const std::size_t room = cap - static_cast<std::size_t>(n) - 2;
    const std::size_t take = msg.size() < room ? msg.size() : room;
    std::memcpy(buf + n, msg.data(), take);
    buf[n + take]     = '\r';
    buf[n + take + 1] = '\n';

    sink_.write({buf, static_cast<std::size_t>(n) + take + 2});
}

void Logger::line(std::string_view msg) noexcept {
    char buf[kLineBufferSize];
    line_in(msg, buf);
}

bool Logger::debug_enabled() const noexcept {
    const bool* flag = debug_flag_.load(std::memory_order_acquire);
    return flag && *flag;
}

void set_process_log(ports::Log* log) noexcept { g_process_log.store(log, std::memory_order_release); }

ports::Log* process_log() noexcept { return g_process_log.load(std::memory_order_acquire); }

void log_line(std::string_view msg) noexcept {
    if (ports::Log* log = process_log()) log->line(msg);
}

void log_line_in(std::string_view msg, char* scratch, std::size_t cap) noexcept {
    if (ports::Log* log = process_log()) log->line_in(msg, {scratch, cap});
}

} // namespace omvr::core::diag
