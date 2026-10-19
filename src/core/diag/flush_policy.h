// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/thread_roles.h"
#include "util/time/steady.h"

#include <chrono>
#include <optional>
#include <string_view>

// When the log file is flushed to disk with Controls.CrashSafeLog on
// (22 §14). Pure.
namespace omvr::core::diag {

// Whether a log line reports a problem: the WRN, ERR and FTL events of
// 11 §1.5. The line format carries no level, so a problem is told by its
// wording: a line that names a failure, an error, a crash, an exception or
// something unexpected, refused, malformed, missing or not applied. Case is
// ignored. No allocation.
bool is_problem_line(std::string_view line) noexcept;

enum class FlushAction : std::uint8_t {
    Deferred, // the mod's thread flushes it within kLogFlushInterval
    Now,      // the writing thread flushes after its write
};

// 22 §14: a problem written on the game (or loader) thread is flushed at
// once; everything else, and anything at all from the render and job
// threads (which never flush and never wait for a flush), is left to the
// mod's thread.
FlushAction flush_action(ThreadRole writer, bool problem) noexcept;

// How long the mod's thread waits, once woken by a write, before it
// flushes: nothing when the last flush was kLogFlushInterval or more ago
// (or there was none), else the rest of that interval, so it flushes at
// most once per interval.
std::chrono::milliseconds flush_delay(util::TimePoint now,
                                      std::optional<util::TimePoint> last_flush) noexcept;

} // namespace omvr::core::diag
