#pragma once

#include "core/diag/ports/log.h"
#include "core/diag/tuning.h"
#include "loader/context.h"

#include <atomic>
#include <exception>
#include <string_view>

namespace omvr::loader::hooks {

// The line a caught exception leaves in the log.
inline constexpr std::string_view kCaughtExceptionLog = "[stereo] exception in {} caught: {}";

// Runs `fn` for a replacement the game calls. A C++ exception from it (a
// backend call, a failed allocation) is caught here and logged, and the
// replacement returns `fallback`: unwinding into game code, which has no
// handler for it, or through a noexcept function (std::terminate) would end
// the process (STYLEGUIDE 6, 7). Each call site (each `fn` type) logs at
// most kGuardLogLimit times, so a call that throws every frame cannot flood
// the log; logf formats into a stack buffer, without allocation.
template <class R, class Fn>
R guarded(const char* where, R fallback, Fn&& fn) noexcept {
    try {
        return fn();
    } catch (...) {
        static std::atomic<int> logged{0};
        // Relaxed: only the count matters, not what it orders.
        if (logged.fetch_add(1, std::memory_order_relaxed) < core::diag::kGuardLogLimit) {
            const char* what = "unknown exception";
            try {
                throw;
            } catch (const std::exception& e) {
                what = e.what();
            } catch (...) {
            }
            try {
                core::diag::ports::logf(context().log, kCaughtExceptionLog, where, what);
            } catch (...) {
            }
        }
        return fallback;
    }
}

// As above, for a replacement that returns nothing.
template <class Fn>
void guarded(const char* where, Fn&& fn) noexcept {
    guarded(where, 0, [&] {
        fn();
        return 0;
    });
}

} // namespace omvr::loader::hooks
