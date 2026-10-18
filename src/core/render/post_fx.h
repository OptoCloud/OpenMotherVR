// OpenMotherVR-Provenance: clean
#pragma once

#include "core/render/types.h"

#include <atomic>
#include <optional>

// The 2D-only effects in stereo (spec 24 §8): lens flares, chromatic
// aberration and film grain, each with a Graphics key that turns it off in
// the headset. Flat play and the player's saved game settings are never
// changed: the game's own fields are overridden only around the reader's
// call while stereo runs.
namespace omvr::core::render {

// An effect is turned off only while stereo runs and its key is false
// (24 §8: flat play is never changed).
constexpr PostFxOff effects_off(const PostFxKeys& keys, bool stereo_running) noexcept {
    if (!stereo_running) return {};
    return {.lens_flares          = !keys.lens_flares,
            .chromatic_aberration = !keys.chromatic_aberration,
            .film_grain           = !keys.film_grain};
}

// The keys in effect, set on the main thread and read by the render
// thread's hooks, and what to log about them (24 §11.3).
class PostFxEffects final {
public:
    // Any thread.
    void set_keys(const PostFxKeys& keys) noexcept;
    PostFxKeys keys() const noexcept;

    // What to turn off this frame. Render thread.
    PostFxOff off(bool stereo_running) const noexcept { return effects_off(keys(), stereo_running); }

    // The keys to log: once when stereo starts, and once per change while
    // it runs; nullopt otherwise (no per-frame logging, 12 §6.1). Render
    // thread only.
    std::optional<PostFxKeys> report(bool stereo_running) noexcept;

private:
    std::atomic<bool> lens_flares_{true};
    std::atomic<bool> chromatic_aberration_{true};
    std::atomic<bool> film_grain_{true};
    // Render thread only.
    bool reported_stereo_ = false;
    PostFxKeys reported_{};
};

} // namespace omvr::core::render
