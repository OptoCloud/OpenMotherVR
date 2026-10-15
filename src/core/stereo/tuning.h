#pragma once

#include <array>
#include <chrono>
#include <cstddef>

// Behaviour parameters of the stereo device, the stereo update and the
// render fixes: the project's choices, each defined once here.
namespace omvr::core::stereo {

// ---- The runtimes' virtual screen (03 §7) ----------------------------------

// The virtual screen for menus, videos and loading: the left eye's image
// this far in front of where the head faced at the last recenter, and this
// wide (both metres).
inline constexpr float kCinemaDistance = 1.5f;
inline constexpr float kCinemaWidth    = 32.0f / 9.0f;

// ---- The game's UI quads (05 §4) -------------------------------------------

// The menu screen (mode 1) stands this far ahead of the camera, in metres.
inline constexpr float kMenuScreenDistance = 1.5f;
// The menu screen's height over its width when the stereo update re-places
// it after a recenter: the game's 16:9 UI.
inline constexpr float kMenuScreenInvAspect = 9.0f / 16.0f;
// The head-locked HUD panel (mode 0) is this wide, in metres; it stands one
// unit forward of the eyes (the view's forward axis has length 1).
inline constexpr float kHudPanelWidth = 0.55f;
// A forward axis shorter than this in the horizontal plane (looking
// straight up or down) has no heading: the screen faces +Z instead.
inline constexpr float kMinForwardLength = 1e-6f;

// ---- The stereo update (05 §3.2) -------------------------------------------

// A turn-axis value beyond this is a snap step (05 §3.2c).
inline constexpr float kTurnThreshold = 0.5f;
// A held stick repeats a snap step this often.
inline constexpr auto kSnapRepeat = std::chrono::milliseconds(300);
// A full turn takes at least this many snap steps, whatever the setting.
inline constexpr int kMinSnapSegments = 2;
// Debug: how long after a snap step the camera is traced.
inline constexpr auto kSnapTraceTime = std::chrono::milliseconds(600);
// After a recenter the menu screen is re-placed in front of the new
// forward on this many updates (05 §3.2e).
inline constexpr int kScreenReplaceFrames = 2;

// ---- Head-in-wall blackout delay (08 §4.2) ---------------------------------

// The fade counts as clear at or above this, and a fade that starts is
// held off this long in stereo.
inline constexpr float kFadeClear = 0.99999f;
inline constexpr auto kFadeGrace  = std::chrono::milliseconds(600);

// ---- Render target (03 §4) -------------------------------------------------

// The side-by-side target stays within D3D11's largest 2D texture
// (D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION), in pixels.
inline constexpr float kMaxTextureSize = 16384.0f;

// ---- Desktop mirror (03 §8) ------------------------------------------------

// RGBA. The window is cleared to this before the mirror draws, so a draw
// that fails shows as red, and to black when the mirror is off.
inline constexpr std::array<float, 4> kMirrorClearColour = {0.6f, 0.0f, 0.0f, 1.0f};
inline constexpr std::array<float, 4> kMirrorOffColour   = {0.0f, 0.0f, 0.0f, 1.0f};
// In screen frames the mirror shows what the virtual screen shows: eye 0's
// half of the stereo texture, its middle half vertically (UV left, top,
// right, bottom).
inline constexpr std::array<float, 4> kScreenMirrorSource = {0.0f, 0.25f, 0.5f, 0.75f};

// ---- Per-eye reflections, diagnostics (07 §6) ------------------------------

// Reflection POVs whose flags are followed in the log, and the most lines
// that diagnostic writes in a run.
inline constexpr std::size_t kReflectionPovSlots = 8;
inline constexpr int kReflectionLogLines         = 60;

} // namespace omvr::core::stereo
