// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/stereo/ports/keyboard.h"
#include "core/stereo/tuning.h"
#include "util/sync/once_latch.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

namespace omvr::core::stereo {

// What the reflection render call shows of a reflection POV, for the
// diagnostics (07 §6).
struct ReflectionPovInfo {
    const void* pov;    // identity only, never dereferenced
    std::uint8_t flags; // POV +0x36B
    int state;          // the device state, -1 without a device
    std::int32_t width; // its render target's size
    std::int32_t height;
    std::int32_t quality = -1; // the reflection quality setting (07 §1, §6), -1 unknown
};

// A viewport in pixels of the target it is set on (07 §4.8): the viewport
// setter's rectangle without its depth range.
struct PixelViewport {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
    friend bool operator==(const PixelViewport&, const PixelViewport&) = default;
};

// The screen-mapping constant of the full-screen and deferred-light vertex
// shaders (07 §2.7): the viewport's size and origin as fractions of the
// target's, (vpW / W, vpH / H, left / W, top / H).
struct ScreenMapping {
    float scale_x;
    float scale_y;
    float offset_x;
    float offset_y;
    friend bool operator==(const ScreenMapping&, const ScreenMapping&) = default;
};

// 07 §4.8: `vp`, set on a target `target_width` pixels wide during the pass
// for half `half` (0 or 1), moved into that half. Half 0 is the columns 0
// to floor(w/2) - 1, half 1 the next floor(w/2) (an odd last column is not
// used). A viewport inside the half is kept; one covering the full width
// becomes the half; one inside the other half (the origin lost, 07 §8 item
// 1) moves over by the half's width; anything else is cut to the half.
// Height and top are always kept. Pure.
PixelViewport confine_viewport(const PixelViewport& vp, std::int32_t target_width, int half) noexcept;

// 07 §4.8: the screen mapping `m` of a target `target_width` pixels wide,
// changed exactly as confine_viewport changes the viewport it describes, so
// that a full-screen pass reads the half it writes. Pure.
ScreenMapping confine_screen_mapping(const ScreenMapping& m, std::int32_t target_width, int half) noexcept;

// Planar reflections (07).
//
// The game renders the floor reflection once, mono, with the flat camera's
// projection, and the floor shader samples it in screen space over the
// whole side-by-side target, so in VR it slides against head motion. With
// stereo running, render it per eye into the two halves of its target
// instead: the reflection view is mirrored in X, so half k rendered with
// eye k's transform is what the other eye samples. Each render clears the
// whole target, so the second pass runs with clears skipped, and each pass
// keeps its viewports and screen mapping inside its own half. In stereo the
// reflection's object list is built with the headset's projection, so it
// covers both eyes. Decisions and diagnostics only: the game side renders.
// Render thread, except where a member says otherwise.
class ReflectionPass final {
public:
    ReflectionPass(const ports::Keyboard& keyboard, const config::Settings& settings,
                   diag::ports::Log& log) noexcept
        : keyboard_(keyboard), settings_(settings), log_(log) {}

    // Whether the game's clear could be gated and the passes kept in their
    // halves (07 §4.3, §4.8); without both the fix stays off. Set once at
    // attach.
    void set_ready(bool ready) noexcept { ready_ = ready; }

    // Whether the reflection update could be redirected, so that the object
    // list covers both eyes (07 §4.7). Set once at attach.
    void set_covers_both_eyes(bool on) noexcept { covers_both_eyes_ = on; }

    // 07 §4.7: after the game's update of an active (`pov_active`)
    // reflection POV while stereo runs, its projection becomes the
    // headset's eye -1 projection. Game thread.
    bool widens_object_list(bool stereo_running, bool pov_active) const noexcept {
        return ready_ && covers_both_eyes_ && stereo_running && pov_active;
    }

    // Logs each reflection POV, every change of its flags and every change
    // of the quality setting, up to kReflectionLogLines lines.
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

    // Pass `half` of the per-eye render runs on the thread `thread` (a
    // thread id, never 0) until end_pass. Render thread.
    void begin_pass(int half, std::uint32_t thread) noexcept;
    void end_pass() noexcept;

    // The half whose pass runs on the calling thread `thread`, if any. Any
    // thread: only the render thread, during a pass, gets one.
    std::optional<int> pass_on(std::uint32_t thread) const noexcept {
        // Relaxed: only the thread that stored its own id can match it, and
        // it wrote half_ before, in program order.
        if (thread == 0 || pass_thread_.load(std::memory_order_relaxed) != thread) return std::nullopt;
        return half_;
    }

    // The viewport pass `half` sets on a target `target_width` pixels wide:
    // `vp` confined to the half (07 §4.8). The first change in a run is
    // logged with the pass, the width and both viewports (07 §6). Render
    // thread.
    PixelViewport pass_viewport(int half, const PixelViewport& vp, std::int32_t target_width) noexcept;

private:
    const ports::Keyboard& keyboard_;
    const config::Settings& settings_;
    diag::ports::Log& log_;
    bool ready_            = false;
    bool covers_both_eyes_ = false;

    // The running pass: its half, written and read on the render thread
    // only, and that thread's id (0: no pass).
    int half_ = 0;
    std::atomic<std::uint32_t> pass_thread_{0};

    // The POVs seen so far and their last flags (-1: new).
    struct SeenPov {
        const void* pov = nullptr;
        int flags       = -1;
    };
    std::array<SeenPov, kReflectionPovSlots> seen_{};
    int lines_             = 0;
    int quality_           = -2; // last logged quality; -2 before the first line
    bool capture_key_down_ = false;
    util::OnceLatch rendered_logged_;
    util::OnceLatch narrowed_logged_;
};

} // namespace omvr::core::stereo
