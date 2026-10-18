#pragma once

#include "game/game_image.h"
#include "game/patcher.h"
#include "game/stereo/stereo_objects.h"

#include <cstdint>

namespace omvr::game::sites {

// 6: the eight "Stereo Mode == Rift" tests in AI.exe's stereo/render path
// (RVAs in InlineSites::stereo_mode_tests, addresses.h), which the
// preflight checked hold the game's `cmp reg, eax`, become `cmp eax, eax`,
// so the game takes its LibOVR stereo path, which our stereo manager
// stands in for (05 §2). Returns false if a write failed.
bool force_stereo_mode(const GameImage& g, Patcher& patcher) noexcept;

// The replacements step 7 installs, and the vtables of the objects it hands
// to the game.
struct StereoDeviceHooks {
    stereo::DeviceSlots device;
    stereo::ManagerSlots manager;
    const void* create_device;
    const void* get_hmd_resolution;
    const void* get_render_target_size;
};

// 7: the game's hidden LibOVR stereo path gets our manager `m` and device
// (02 §3.1, §6.1, §6.2, §6.4).
bool install_stereo_device(const GameImage& g, Patcher& patcher, StereoManager& m,
                           const StereoDeviceHooks& hooks) noexcept;

// Sites step 7 writes: hand-off, two recenter sites, three calls.
inline constexpr int kStereoDeviceSites = 6;

} // namespace omvr::game::sites
