#pragma once

#include "game/game_image.h"
#include "game/patcher.h"
#include "game/stereo/stereo_objects.h"

#include <cstdint>

namespace omvr::game::sites {

// The eight "Stereo Mode == Rift" tests in AI.exe's stereo/render path
// (RVAs in InlineSites::stereo_mode_tests, addresses.h). The hook
// installer's preflight checks each holds the game's `cmp reg, eax`.
//
// Rewrite each test as `cmp eax, eax` so it always passes and the game takes
// its LibOVR stereo path, which our stereo manager stands in for (05 §2).
// Returns false if a write failed.
// From: FUN_1001d6f0 lines 72..100 (payload g_payload_1005dc34).
bool force_stereo_mode_tests(const GameImage& g, Patcher& patcher) noexcept;

// 6
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
