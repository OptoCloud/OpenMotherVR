#pragma once

#include <cstdint>

// Values that cross the stereo ports.
namespace omvr::core::stereo {

// The game's swapchain (IDXGISwapChain* on the platform side).
enum class SwapchainHandle : std::uintptr_t {};

// The constant buffer of mirror.hlsl: per panel, the draw rectangle in
// window UV and the source region in stereo-texture UV, each as left, top,
// right, bottom (v down). Panel 1 is empty except for "both eyes" cropped.
struct MirrorConstants {
    float draw[2][4];
    float source[2][4];
};
static_assert(sizeof(MirrorConstants) == 0x40);

} // namespace omvr::core::stereo
