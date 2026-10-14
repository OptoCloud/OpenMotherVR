#pragma once

struct IDXGISwapChain;

namespace mothervr::stereo {

// Called from the swapchain Present hook on every frame, before the real
// Present. Drives the OpenXR backend, which until it gets a stereo device of
// its own shows the game's finished frame on a virtual screen. (OpenVR runs
// through the stereo device, see stereo_device.h.)
//
// Returns true when the OpenXR virtual screen is pacing this frame (it has
// already blocked in xrWaitFrame): the caller must then present with
// SyncInterval 0, or the game waits for both the headset and the monitor
// and judders at the slower of the two rates.
bool on_present(IDXGISwapChain* swapchain) noexcept;

} // namespace mothervr::stereo
