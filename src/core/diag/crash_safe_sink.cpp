// OpenMotherVR-Provenance: clean
#include "core/diag/crash_safe_sink.h"

#include "core/diag/flush_policy.h"

#include <string_view>

namespace omvr::core::diag {

void CrashSafeSink::enable() noexcept {
    enabled_.store(true, std::memory_order_release);
    flush_.flush_soon();
}

void CrashSafeSink::write(std::span<const char> line) noexcept {
    file_.write(line);
    if (!enabled()) return;
    const ThreadRole writer = roles_.role_of(thread_.id());
    const bool problem      = is_problem_line(std::string_view{line.data(), line.size()});
    if (flush_action(writer, problem) == FlushAction::Now)
        flush_.flush_now();
    else
        flush_.flush_soon();
}

} // namespace omvr::core::diag
