#pragma once

#include <filesystem>

#include <windows.h>

namespace mothervr::hook {

// Install all mod patches and hooks into the running AI.exe process.
// Must be called after config::initialize(). `self` is this DLL (its
// resources hold the payloads), `game_dir` the Alien: Isolation install dir.
//
// Steps, in the original's order:
//   1. Select and rebase the AI.exe address table for the detected build.
//      Then a preflight checks the code and data at every core site.
//   2. Deploy DATA\MOTHER.PAK and redirect the game's UI.PAK path to it.
//   3. Deploy the bundled openvr_api.dll when the runtime is OpenVR.
//   4. Skip-intro patch ("ShowBootup" -> "SkipBootup") when configured.
//   5. Script-variable callback pointers + console-command dispatch hook.
//   6. "Stereo Mode == Rift" tests forced true.
//   7. Stereo manager/device handed to the game.
//   8. Game-side stereo hooks: call redirects into the stereo manager, the
//      FollowBehavior, UI entity and menu/loading screen vtable slots.
//   8b. Fixes the original lacks (planar reflections, anti-aliasing,
//      culling, head-in-wall fade), each skipped on its own when its code
//      does not match.
//   9. Camera, UI and loader byte patches; gamepad vtable hooks (VR input).
//
// From: FUN_1001d6f0 @ 0x1001d6f0 (the original monolithic hook installer).
//
// Returns false, with nothing patched, if the AI.exe build is unknown or a
// core site does not hold the expected code. A missing UI archive only
// disables the options page (the original bails out there). Also returns
// false if a write fails after the preflight passed; the patches made before
// it stay in place.
bool install(HMODULE self, const std::filesystem::path& game_dir) noexcept;

} // namespace mothervr::hook
