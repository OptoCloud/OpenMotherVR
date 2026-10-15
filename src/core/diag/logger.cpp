// OpenMotherVR-Provenance: clean
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

#include <array>

namespace omvr::core::diag {

void Logger::line_in(std::string_view msg, std::span<char> scratch) noexcept {
    if (!sink_.ready()) return;
    const std::size_t n = format_line(scratch, clock_.local_time(), pid_, msg);
    if (n == 0) return;
    sink_.write({scratch.data(), n});
}

void Logger::line(std::string_view msg) noexcept {
    std::array<char, kLineBufferSize> buf;
    line_in(msg, buf);
}

bool Logger::debug_enabled() const noexcept {
    const bool* flag = debug_flag_.load(std::memory_order_acquire);
    return flag && *flag;
}

} // namespace omvr::core::diag
