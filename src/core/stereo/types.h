#pragma once

#include "game/constants.h"

#include <array>
#include <cstdint>
#include <optional>

// Values that cross the stereo ports and the stereo thunks: the thunks read
// the game into these, the stereo logic decides over them, and the thunks
// write the results back.
namespace omvr::core::stereo {

// The game's swapchain (IDXGISwapChain* on the platform side).
enum class SwapchainHandle : std::uintptr_t {};

// A 4x4 matrix as the game stores it: 16 floats, row-major, row vectors
// (the translation is row 3).
using Matrix44 = std::array<float, 16>;

// A size in pixels.
struct PixelSize {
    std::int32_t width  = 0;
    std::int32_t height = 0;

    friend bool operator==(const PixelSize&, const PixelSize&) = default;
};

// A rectangle in pixels: left, top, width, height (LibOVR's Recti order).
struct PixelRect {
    std::int32_t x      = 0;
    std::int32_t y      = 0;
    std::int32_t width  = 0;
    std::int32_t height = 0;
};

// The two eye halves of a side-by-side target: eye 0 left, eye 1 right.
using EyeViewports = std::array<PixelRect, 2>;

// The constant buffer of mirror.hlsl: per panel, the draw rectangle in
// window UV and the source region in stereo-texture UV, each as left, top,
// right, bottom (v down). Panel 1 is empty except for "both eyes" cropped.
struct MirrorConstants {
    float draw[2][4];
    float source[2][4];
};
static_assert(sizeof(MirrorConstants) == 0x40);

// The name hashes of the UI entity messages that start and finish a
// cutscene or a level fade (05 §6.2): game data, read by the thunk.
struct SceneMessages {
    std::uint32_t cutscene_start  = 0;
    std::uint32_t cutscene_finish = 0;
    std::uint32_t fade_start      = 0;
    std::uint32_t fade_finish     = 0;
};

// ---- Game facts the stereo logic decides over ------------------------------
//
// Defined in game/constants.h (core may read that header); named here in
// the stereo logic's terms.

// The stereo mode at StereoManager +0x60 and its stack (05 §4.4): the 3D
// world with the HUD panel, or the virtual screen (menus, videos, loading).
// The game may hand other values to PushMode; they pass through unchanged.
enum class StereoMode : std::int32_t {
    World  = game::kStereoModeWorld,
    Screen = game::kStereoModeScreen,
};

using game::AaMode;
using game::GraphicsOption;
using game::kSceneBlack;
using game::kSceneVisible;

// ---- The mode stack (05 §4.4) -------------------------------------------------

// What the game's mode stack must do: leave mode 1 for the world, or pop
// the top, then optionally push a mode. The game side runs the steps in
// that order through the game's own push and pop (game/stereo/mode_stack).
struct ModeSteps {
    bool pop_to_world = false;
    bool pop          = false;
    std::optional<StereoMode> push;
};

// The mode stack as the game holds it (StereoManager +0x60, +0x68..+0x70).
struct ModeStackState {
    StereoMode mode       = StereoMode::World;
    std::int32_t count    = 0;
    std::int32_t capacity = 0;
    bool has_storage      = false; // the array has a data pointer
};

// How the end of an update brings the stack count to the mode (05 §3.2f).
enum class StackSync {
    None,         // already equal
    Lower,        // write the count down to the mode
    Append,       // append mode 1 through the game's routine
    WriteInPlace, // no routine: write 1 into storage the array already has
    NoStorage,    // no routine and no storage: leave the count (logged once)
};

// How a sync the game side carried out went.
enum class StackSyncOutcome {
    Done,
    AppendFailed, // the game's append failed to allocate; count put back to 0
    NoStorage,
};

} // namespace omvr::core::stereo
