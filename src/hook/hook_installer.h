#pragma once

#include <filesystem>

#include <windows.h>

namespace mothervr::hook {

// Install all mod patches and hooks into the running AI.exe process.
// Must be called after config::initialize(). `self` is this DLL (its
// resources hold the payloads), `game_dir` the Alien: Isolation install dir.
//
// The patch set follows the runtime for this run (mothervr.json's
// VRRuntime, or a -vrforce* switch): with runtime None only the UI set is
// patched (steps 2, 4, 5 and the pause menu's settings push), so flat play
// is the unmodded game plus the options page.
//
// Steps, in the original's order:
//   1. Select and rebase the AI.exe address table for the detected build.
//      Then a preflight checks the code and data at every core site of the
//      patch set, logging each mismatch with its RVA and what was found.
//   2. Deploy DATA\MOTHER.PAK and redirect the game's UI.PAK path to it.
//   3. (Moved) The original deploys openvr_api.dll here; the OpenVR backend
//      now does it when it starts, outside the loader lock.
//   4. Skip-intro patch ("ShowBootup" -> "SkipBootup") when configured and
//      step 2 redirected the archive (only the mod's movie defines it).
//   5. Script-variable callback pointers + console-command dispatch hook.
//   6. "Stereo Mode == Rift" tests forced true.
//   7. Stereo manager/device handed to the game.
//   8. Game-side stereo hooks: call redirects into the stereo manager, the
//      FollowBehavior, UI entity and menu/loading screen vtable slots.
//   8b. Fixes the original lacks (planar reflections, anti-aliasing,
//      culling, head-in-wall fade), each skipped on its own when its code
//      does not match.
//   9. Camera, UI and loader byte patches; gamepad vtable hooks (VR input),
//      then, only if those are in place, the pad-connected patch and the
//      left-stick prompt redirects.
//
// From: FUN_1001d6f0 @ 0x1001d6f0 (the original monolithic hook installer).
//
// Returns false, with nothing patched, if the AI.exe build is unknown or a
// core site does not hold the expected code. A missing UI archive only
// disables the options page (the original bails out there). Also returns
// false if a write fails after the preflight passed; the patches made before
// it stay in place.
bool install(HMODULE self, const std::filesystem::path& game_dir) noexcept;

// True when the runtime for this run is None, so install() patched only the
// UI set: the D3D11 proxy then wraps neither the game's device nor its
// swapchain. Decided by install() before anything else; false before.
bool ui_set_only() noexcept;

} // namespace mothervr::hook
