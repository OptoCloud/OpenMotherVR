// OpenMotherVR-Provenance: clean
#include "core/diag/flush_policy.h"

#include "core/diag/tuning.h"
#include "util/text/ascii.h"

#include <array>
#include <cstddef>

namespace omvr::core::diag {

namespace {

// The words that mark a line as a warning or an error (lower case). They
// cover the mod's failure lines: "... failed", "err=0x...", "cannot ...",
// "crash: ...", "... caught", "unexpected code", "... ignored", "not
// patched", "running without VR" and the like.
constexpr std::array<std::string_view, 17> kProblemWords = {
    "fail",      "error",       "err=",         "cannot",         "can't",      "crash",
    "exception", "unexpected",  "refused",      "malformed",      "warning",    "not found",
    "ignored",   "not patched", "not detoured", "not redirected", "without vr",
};

} // namespace

bool is_problem_line(std::string_view line) noexcept {
    for (const std::string_view word : kProblemWords)
        if (util::contains_ignoring_ascii_case(line, word)) return true;
    return false;
}

FlushAction flush_action(ThreadRole writer, bool problem) noexcept {
    return writer == ThreadRole::Main && problem ? FlushAction::Now : FlushAction::Deferred;
}

std::chrono::milliseconds flush_delay(util::TimePoint now,
                                      std::optional<util::TimePoint> last_flush) noexcept {
    if (!last_flush) return std::chrono::milliseconds{0};
    const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - *last_flush);
    return since >= kLogFlushInterval ? std::chrono::milliseconds{0} : kLogFlushInterval - since;
}

} // namespace omvr::core::diag
