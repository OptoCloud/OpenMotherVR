#pragma once

#include "core/stereo/ports/keyboard.h"
#include "game/stereo/pov.h" // R: stereo (A3): the targets stay in the game layer's thunk side

namespace omvr::core::stereo {

// Planar reflections (not in the original).
//
// The game renders the floor reflection once, mono, with the flat camera's
// projection, and the floor shader samples it in screen space over the
// whole side-by-side target, so in VR it slides against head motion. With
// stereo running, render it per eye into the two halves of its target
// instead: the reflection view is mirrored in X, so half k rendered with
// eye k's transform is what the other eye samples. Each render clears the
// whole target, so the second pass runs with clears skipped.
// Analysis: dumps/07_reflections_head_pose.md. Render thread.
class ReflectionPass {
public:
    ReflectionPass(game::StereoManager& manager, const ports::Keyboard& keyboard) noexcept
        : manager_(manager), keyboard_(keyboard) {}

    // False if the clear could not be gated (its code differs from the
    // expected build); the fix then stays off.
    bool install(const game::stereo::ReflectionTargets& targets) noexcept;

    // Replaces the call of ReflectionPOV render on `pov`.
    void render(void* pov) noexcept;

private:
    game::StereoManager&               manager_;
    const ports::Keyboard&             keyboard_;
    game::stereo::ReflectionTargets    g_reflection{};
    bool                               g_reflection_ready = false;
};

} // namespace omvr::core::stereo
