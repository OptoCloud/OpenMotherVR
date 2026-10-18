// OpenMotherVR-Provenance: clean
#include "core/render/post_fx.h"

namespace omvr::core::render {

// Relaxed: each key is a value on its own; nothing else is published with it.
void PostFxEffects::set_keys(const PostFxKeys& keys) noexcept {
    lens_flares_.store(keys.lens_flares, std::memory_order_relaxed);
    chromatic_aberration_.store(keys.chromatic_aberration, std::memory_order_relaxed);
    film_grain_.store(keys.film_grain, std::memory_order_relaxed);
}

PostFxKeys PostFxEffects::keys() const noexcept {
    return {.lens_flares          = lens_flares_.load(std::memory_order_relaxed),
            .chromatic_aberration = chromatic_aberration_.load(std::memory_order_relaxed),
            .film_grain           = film_grain_.load(std::memory_order_relaxed)};
}

std::optional<PostFxKeys> PostFxEffects::report(bool stereo_running) noexcept {
    const bool started = stereo_running && !reported_stereo_;
    reported_stereo_   = stereo_running;
    if (!stereo_running) return std::nullopt;
    const PostFxKeys now = keys();
    if (!started && now == reported_) return std::nullopt;
    reported_ = now;
    return now;
}

} // namespace omvr::core::render
