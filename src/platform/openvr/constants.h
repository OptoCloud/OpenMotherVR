#pragma once

#include <openvr.h>

#include <cstdint>

// OpenVR values the SDK headers do not name.
namespace omvr::platform::openvr {

// Legacy controller state button bits.
inline constexpr std::uint64_t kButtonMenu     = 1ull << vr::k_EButton_ApplicationMenu;
inline constexpr std::uint64_t kButtonGrip     = 1ull << vr::k_EButton_Grip;
inline constexpr std::uint64_t kButtonTouchpad = 1ull << vr::k_EButton_SteamVR_Touchpad;

// Bumped whenever actions are added (2: the hand poses). SteamVR reads the
// manifest we rewrite each launch, so new actions and the default bindings
// for them arrive by themselves; the number marks which manifest a binding
// file was made for, so a binding a player saved from an older version can
// be told apart in SteamVR's binding UI (it lacks the new actions, which
// then read inactive: see Actions::hand_poses()).
inline constexpr int kManifestVersion = 2;

} // namespace omvr::platform::openvr
