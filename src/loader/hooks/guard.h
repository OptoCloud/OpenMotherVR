#pragma once

#include "core/diag/logger.h"
#include "core/diag/tuning.h"

#include <atomic>
#include <exception>

namespace omvr::loader::hooks {

namespace diag = core::diag;

// Runs `fn` for a replacement the game calls. A C++ exception from it (a
// backend call, a failed allocation) is caught here and logged, and the
// replacement returns `fallback`: unwinding into game code, which has no
// handler for it, or through a noexcept function (std::terminate) would end
// the process. Each call site (each `fn` type) logs at most 16 times, so a
// call that throws every frame cannot flood the log; logf formats into a
// stack buffer, without allocation.
template <class R, class Fn>
R guarded(const char* where, R fallback, Fn&& fn) noexcept {
    try {
        return fn();
    } catch (...) {
        static std::atomic<int> s_logged{0};
        if (s_logged.fetch_add(1, std::memory_order_relaxed) < diag::kGuardLogLimit) {
            const char* what = "unknown exception";
            try {
                throw;
            } catch (const std::exception& e) {
                what = e.what();
            } catch (...) {
            }
            try {
                diag::logf("[stereo] exception in {} caught: {}", where, what);
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
