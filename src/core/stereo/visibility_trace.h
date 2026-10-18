// OpenMotherVR-Provenance: clean
#pragma once

#include "core/stereo/tuning.h"

#include <algorithm>

namespace omvr::core::stereo {

// Dev. Debug Options: notes when a scene visibility (1 clear, 0 black)
// leaves clear and when it comes back, and the lowest value in between, so
// the log shows which of the writers darkened the view and when, without a
// line per frame. Pure; one per traced value, game thread.
class VisibilityTrace final {
public:
    enum class Change { None, Darkened, Cleared };

    Change update(float visibility) noexcept {
        const bool dark = visibility < kFadeClear;
        if (dark) lowest_ = dark_ ? std::min(lowest_, visibility) : visibility;
        if (dark == dark_) return Change::None;
        dark_ = dark;
        return dark ? Change::Darkened : Change::Cleared;
    }

    // update(), then on a change calls `darkened()` or `cleared(lowest())`:
    // the two log lines of a traced value.
    template <class Darkened, class Cleared>
    void report(float visibility, Darkened&& darkened, Cleared&& cleared) noexcept {
        switch (update(visibility)) {
            case Change::Darkened: darkened(); break;
            case Change::Cleared: cleared(lowest_); break;
            case Change::None: break;
        }
    }

    // The lowest value since the last Darkened.
    float lowest() const noexcept { return lowest_; }

private:
    bool dark_    = false;
    float lowest_ = 1.0f;
};

} // namespace omvr::core::stereo
