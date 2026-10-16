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
// Smooth turn (spec 22 §4.2): the turn value's centre zone, the same one
// the VR turn value already has (04 §5.8.5), so a real stick at rest never
// drifts; and the longest time one update turns for, so a hitch does not
// jump the view (below 20 frames per second the turn slows instead).
inline constexpr float kSmoothTurnDeadZone = 0.33f;
inline constexpr auto kSmoothTurnMaxStep   = std::chrono::milliseconds(50);
// A full turn takes at least this many snap steps, whatever the setting.
inline constexpr int kMinSnapSegments = 2;
// Debug: how long after a snap step the camera is traced.
inline constexpr auto kSnapTraceTime = std::chrono::milliseconds(600);
// After a recenter the menu screen is re-placed in front of the new
// forward on this many updates (05 §3.2e).
inline constexpr int kScreenReplaceFrames = 2;

// ---- Comfort vignette and snap-turn fade (spec 22 §4.3, §5) ----------------

// The merged movement value drives the vignette above this magnitude, so
// stick noise does not.
inline constexpr float kVignetteMoveThreshold = 0.1f;
// The shown strength takes this long from 0 to full (a fast onset covers
// the start of motion) and from full to 0 (a slower release avoids flicker
// when the stick is briefly let go).
inline constexpr auto kVignetteRiseTime = std::chrono::milliseconds(150);
inline constexpr auto kVignetteFallTime = std::chrono::milliseconds(300);
// Inner radius in degrees from the eye's forward axis: kVignetteInnerDeg
// minus kVignetteInnerPerStrengthDeg times the strength; the outer radius
// is kVignetteBandDeg further out. Full strength leaves a clear 25 degrees.
inline constexpr float kVignetteInnerDeg            = 60.0f;
inline constexpr float kVignetteInnerPerStrengthDeg = 35.0f;
inline constexpr float kVignetteBandDeg             = 15.0f;
// Each snap step's frame is black, then the view returns over this long.
inline constexpr auto kSnapFadeTime = std::chrono::milliseconds(100);

// ---- Head-in-wall blackout delay (08 §4.2) ---------------------------------

// The fade counts as clear at or above this, and a fade that starts is
// held off this long in stereo.
inline constexpr float kFadeClear = 0.99999f;
inline constexpr auto kFadeGrace  = std::chrono::milliseconds(600);
// HeadClipPivot: how much further the game's eye may be from the body's
// pivot than the real head has moved since the recenter before the rest is
// taken as an animation driving the view (metres). Above the few
// centimetres a neck model or the game's own camera sway add, well below
// the 0.5 m where the fade starts.
inline constexpr float kHeadClipAnimationMargin = 0.2f;
// HeadClipPivot (in play, see game_drives_view for scenes): while the real
// head is within this distance of where it
// was at the last recenter, it cannot be inside a wall (the game's own
// collision keeps the body, and the eye with it, out of geometry), so the
// game's fade is not used. Scenes where an animation puts the character's
// face against a wall no longer black out. Metres.
inline constexpr float kHeadClipStillRadius = 0.15f;

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

// ---- VR-off hotkey (22 §13) ------------------------------------------------

// How long the chord must be held before VR turns off: long enough that a
// brushed key does nothing.
inline constexpr auto kVrOffHold = std::chrono::milliseconds(1000);

} // namespace omvr::core::stereo
