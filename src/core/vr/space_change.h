// OpenMotherVR-Provenance: clean
#pragma once

#include <atomic>
#include <optional>

namespace omvr::core::vr {

// The runtime moved its tracking space under the mod (OpenVR: a new
// chaperone universe, or the seated or standing zero pose reset; 03 §6.8):
// the reference pose and the virtual screen placed from it are stale, and a
// retake is queued (03 §2.2).
//
// The render thread notes a change when it reads the runtime's events,
// before the frame's pose wait, and publishes it right after that wait, so
// any pose the game thread reads once it has taken the request comes from a
// wait that ran after the change was reported. Several changes before a
// take count as one, as a queued retake is a single flag.
class SpaceChange {
public:
    // Render thread: the runtime reported a change.
    void note() noexcept { noted_ = true; }
    // Render thread, right after the pose wait: hand a noted change over.
    void publish() noexcept;
    // Game thread: true once for the changes published since the last take.
    [[nodiscard]] bool take() noexcept { return pending_.exchange(false, std::memory_order_acq_rel); }

private:
    bool noted_ = false; // render thread only
    std::atomic<bool> pending_{false};
};

// How a frame's kind (head-tracked or screen, 03 §6.2) is logged.
enum class FrameKindLine {
    None,   // unchanged, or a later change without DebugOptions
    Always, // the first frame, or a change before the first head-tracked frame
    Debug,  // a later change, with DebugOptions on
};

// 11 §4.5: the first frame's kind and each change up to and including the
// first head-tracked frame are logged whatever DebugOptions says, so a log
// shows whether the start-up videos, the legal screen and the front end
// were on the virtual screen; later changes only with DebugOptions. Render
// thread.
class FrameKindLog {
public:
    [[nodiscard]] FrameKindLine next(bool head_tracked, bool debug) noexcept;

private:
    std::optional<bool> last_;
    bool tracked_seen_ = false;
};

} // namespace omvr::core::vr
