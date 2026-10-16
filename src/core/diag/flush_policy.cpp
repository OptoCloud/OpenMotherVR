// OpenMotherVR-Provenance: clean
#include "core/diag/flush_policy.h"

#include "core/diag/tuning.h"

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

char lower(char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

// `word` (lower case) appears in `text` at `at`, ignoring case.
bool matches_at(std::string_view text, std::size_t at, std::string_view word) noexcept {
    for (std::size_t i = 0; i < word.size(); ++i)
        if (lower(text[at + i]) != word[i]) return false;
    return true;
}

bool contains_word(std::string_view text, std::string_view word) noexcept {
    if (word.size() > text.size()) return false;
    for (std::size_t at = 0; at + word.size() <= text.size(); ++at)
        if (matches_at(text, at, word)) return true;
    return false;
}

} // namespace

bool is_problem_line(std::string_view line) noexcept {
    for (const std::string_view word : kProblemWords)
        if (contains_word(line, word)) return true;
    return false;
}

FlushAction flush_action(ThreadRole writer, bool problem) noexcept {
    return writer == ThreadRole::Main && problem ? FlushAction::Now : FlushAction::Deferred;
}

std::chrono::milliseconds flush_delay(
    std::chrono::steady_clock::time_point now,
    std::optional<std::chrono::steady_clock::time_point> last_flush) noexcept {
    if (!last_flush) return std::chrono::milliseconds{0};
    const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - *last_flush);
    return since >= kLogFlushInterval ? std::chrono::milliseconds{0} : kLogFlushInterval - since;
}

} // namespace omvr::core::diag
