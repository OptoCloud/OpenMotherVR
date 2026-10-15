#pragma once

#include "game/layout.h"

#include <atomic>

// The mod's own region of the stereo device, +0x1F4..+0x22F, after the
// game's part (game/layout.h). The game never reads it; its offsets and the
// device's 0x230 size are fixed so the original's analysis still applies.
namespace omvr::game {

static_assert(offsetof(StereoInterface, backend) == 0x1F4);
static_assert(offsetof(StereoInterface, world_scale) == 0x204);
static_assert(offsetof(StereoInterface, mirror_width) == 0x20C);
static_assert(offsetof(StereoInterface, pose_fetched) == 0x208);
static_assert(offsetof(StereoInterface, mirror_vs) == 0x21C);
static_assert(offsetof(StereoInterface, mirror_sampler) == 0x22C);
static_assert(sizeof(StereoInterface) == 0x230);

// The backend is read on the game thread (head pose, recenter) and the
// render thread (frames), and a late detach clears it on the render thread
// (stereo_device.cpp, fail_runtime). A backend that was in use when it
// was cleared is never deleted, so a call already under way finishes on a
// live object; a call that loads null afterwards does nothing.
inline core::vr::ports::VrRuntime* load_backend(StereoInterface& si) noexcept {
    return std::atomic_ref(si.backend).load(std::memory_order_acquire);
}
inline void store_backend(StereoInterface& si, core::vr::ports::VrRuntime* b) noexcept {
    std::atomic_ref(si.backend).store(b, std::memory_order_release);
}

// Was the head pose requested since the last submit (slot 11 on the game
// thread sets it, slot 6 on the render thread clears it)? Release/acquire
// so a request made before the frame's begin is seen there.
inline bool load_pose_fetched(StereoInterface& si) noexcept {
    return std::atomic_ref(si.pose_fetched).load(std::memory_order_acquire) != 0;
}
inline void store_pose_fetched(StereoInterface& si, bool fetched) noexcept {
    std::atomic_ref(si.pose_fetched).store(fetched ? 1 : 0, std::memory_order_release);
}

} // namespace omvr::game
