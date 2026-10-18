// OpenMotherVR-Provenance: clean
#pragma once

#include <optional>
#include <utility>

namespace omvr::util {

// Remembers the last value seen and says whether a new one differs: "log
// once per change". The first value counts as a change. One thread.
template <class T>
class ChangeLatch {
public:
    ChangeLatch() = default;
    // Starts as if `initial` had been seen: only a different value is a
    // change.
    explicit ChangeLatch(T initial) : last_(std::move(initial)) {}

    [[nodiscard]] bool changed(const T& value) {
        if (last_ && *last_ == value) return false;
        last_ = value;
        return true;
    }

private:
    std::optional<T> last_;
};

} // namespace omvr::util
