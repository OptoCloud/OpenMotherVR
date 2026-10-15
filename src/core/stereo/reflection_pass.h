// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/stereo/ports/keyboard.h"
#include "core/stereo/tuning.h"
#include "util/sync/once_latch.h"

#include <array>
#include <cstdint>

namespace omvr::core::stereo {

// What the reflection render call shows of a reflection POV, for the
// diagnostics (07 §6).
struct ReflectionPovInfo {
    const void* pov;    // identity only, never dereferenced
    std::uint8_t flags; // POV +0x36B
    int state;          // the device state, -1 without a device
    std::int32_t width; // its render target's size
    std::int32_t height;
};

// Planar reflections (07).
//
// The game renders the floor reflection once, mono, with the flat camera's
// projection, and the floor shader samples it in screen space over the
// whole side-by-side target, so in VR it slides against head motion. With
// stereo running, render it per eye into the two halves of its target
// instead: the reflection view is mirrored in X, so half k rendered with
// eye k's transform is what the other eye samples. Each render clears the
// whole target, so the second pass runs with clears skipped. Decisions and
// diagnostics only: the game side renders. Render thread.
class ReflectionPass final {
public:
    ReflectionPass(const ports::Keyboard& keyboard, const config::Settings& settings,
                   diag::ports::Log& log) noexcept
        : keyboard_(keyboard), settings_(settings), log_(log) {}

    // Whether the game's clear could be gated (its code is the expected
    // one); without it the fix stays off. Set once at attach.
    void set_ready(bool ready) noexcept { ready_ = ready; }

    // Logs each reflection POV and every change of its flags, up to
    // kReflectionLogLines lines.
    void note_pov(const ReflectionPovInfo& pov) noexcept;

    // The POV renders per eye: the fix is in, stereo runs, and the POV is
    // active and renders (`pov_renders`). Otherwise it renders flat, as the
    // game does.
    bool per_eye(bool stereo_running, bool pov_renders) const noexcept {
        return ready_ && stereo_running && pov_renders;
    }

    // Debug (F10, with debug options on): capture each eye's pass this
    // time. True on the frame the key goes down. Per-eye frames only.
    bool take_capture_request() noexcept;

    // After the first per-eye render: logged once.
    void note_rendered() noexcept;

private:
    const ports::Keyboard& keyboard_;
    const config::Settings& settings_;
    diag::ports::Log& log_;
    bool ready_ = false;

    // The POVs seen so far and their last flags (-1: new).
    struct SeenPov {
        const void* pov = nullptr;
        int flags       = -1;
    };
    std::array<SeenPov, kReflectionPovSlots> seen_{};
    int lines_             = 0;
    bool capture_key_down_ = false;
    util::OnceLatch rendered_logged_;
};

} // namespace omvr::core::stereo
