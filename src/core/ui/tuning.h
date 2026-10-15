#pragma once

#include <chrono>

namespace omvr::core::ui {

// The lock the processes of one game launch share while one builds
// MOTHER.PAK, and how long another waits for it (spec 14 §5.8): well above
// a rebuild, and short enough that a stuck process does not hold up the
// game's start noticeably.
inline constexpr const wchar_t* kArchiveLockName = L"Local\\MotherVR.MotherPak";
inline constexpr std::chrono::milliseconds kArchiveLockWait{5000};

// Runtime error codes the failure message tells apart (the runtimes' own
// values): SteamVR's VRInitError_Init_AnotherAppLaunching, which its
// Desktop Game Theatre causes, and XR_ERROR_RUNTIME_UNAVAILABLE, no
// loadable OpenXR runtime.
inline constexpr int kOpenVrAnotherAppLaunching = 117;
inline constexpr int kOpenXrRuntimeUnavailable  = -51;

} // namespace omvr::core::ui
