// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/activity.h"

#include <optional>

// When the game hands control back (spec 21 §12): an episode in which the
// game, not the player, has the body (a cutscene or level fade, a loading
// screen, a death, a camera taken by another camera, an interaction, a
// hiding place, a menu over the world) ends when first-person play resumes.
// Read from the same activity the hands use (21 §4.4), computed as if
// tracked hands were on. Game thread.
namespace omvr::core::hands {

class HandBack final {
public:
    // This frame's activity. Returns the episode that ended this frame, the
    // reason it started with, or none.
    std::optional<Activity> update(Activity a) noexcept;

    // An episode is running.
    bool in_episode() const noexcept { return episode_.has_value(); }

private:
    std::optional<Activity> episode_;
};

// Whether a reason of `activity` takes the body from the player for
// 21 §12 (an episode, a death, the virtual screen, no player), rather than
// a feature or the stereo device being off.
bool takes_body(Activity a) noexcept;

} // namespace omvr::core::hands
