// OpenMotherVR-Provenance: clean
#include "core/hands/hand_back.h"

namespace omvr::core::hands {

bool takes_body(Activity a) noexcept {
    switch (a) {
        case Activity::Active:
        case Activity::Off:
        case Activity::NoStereo: return false;
        default: return true;
    }
}

std::optional<Activity> HandBack::update(Activity a) noexcept {
    if (takes_body(a)) {
        // The first reason names the episode: a death that loads is a death.
        if (!episode_) episode_ = a;
        return std::nullopt;
    }
    const std::optional<Activity> ended = a == Activity::Active ? episode_ : std::nullopt;
    episode_.reset();
    return ended;
}

} // namespace omvr::core::hands
