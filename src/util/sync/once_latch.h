#pragma once

#include <atomic>

namespace omvr::util {

// True the first time it is asked, false after: "log this once". Safe from
// any thread; exactly one caller sees true.
class OnceLatch {
public:
    bool first() noexcept { return !done_.exchange(true, std::memory_order_relaxed); }

private:
    std::atomic<bool> done_{false};
};

} // namespace omvr::util
