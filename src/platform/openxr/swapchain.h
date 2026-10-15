#pragma once

// A runtime swapchain the game's images are copied into (09 §5.2).
// Internal to this adapter: only its .cpp files include it.

#include "platform/openxr/xr_handle.h"
#include "platform/openxr/xr_headers.h"

#include <cstdint>
#include <vector>

namespace omvr::platform::openxr {

// The last result of each call made on a swapchain every frame, so a call
// that keeps failing is logged when its result changes, not every frame
// (11 §3). Kept when the swapchain is destroyed and recreated.
struct ChainResults {
    XrResult create  = XR_SUCCESS;
    XrResult images  = XR_SUCCESS;
    XrResult acquire = XR_SUCCESS;
    XrResult wait    = XR_SUCCESS;
    XrResult release = XR_SUCCESS;
    DXGI_FORMAT no_format = DXGI_FORMAT_UNKNOWN;   // source format already logged as unmatched
};

struct Swapchain {
    SwapchainOwner handle;
    std::uint32_t width = 0, height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    std::vector<XrSwapchainImageD3D11KHR> images;
    ChainResults results{};
};

} // namespace omvr::platform::openxr
