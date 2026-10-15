#include "platform/openxr/swapchain.h"

#include "core/diag/logger.h" // R: vr (A1): log through an injected Log&
#include "platform/openxr/session.h"

#include <algorithm>

namespace omvr::platform::openxr {

namespace diag = core::diag;

// Swapchain format whose images the source can be copied into
// (CopySubresourceRegion needs the same typeless family). sRGB variants are
// preferred: the game's back buffer holds gamma-encoded values, and an sRGB
// swapchain tells the compositor exactly that.
DXGI_FORMAT Session::pick_format(DXGI_FORMAT src) {
    std::vector<DXGI_FORMAT> want;
    switch (src) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            want = {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM};
            break;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            want = {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM};
            break;
        default:
            want = {src};
    }
    for (DXGI_FORMAT f : want) {
        for (std::int64_t have : g.formats) {
            if (have == static_cast<std::int64_t>(f)) return f;
        }
    }
    return DXGI_FORMAT_UNKNOWN;
}

void Session::destroy_chain(Swapchain& sc) {
    if (sc.handle != XR_NULL_HANDLE) xrDestroySwapchain(sc.handle);
    const ChainResults results = sc.results;
    sc = {};
    sc.results = results;
}

bool Session::ensure_chain(Swapchain& sc, DXGI_FORMAT src_format, std::uint32_t w, std::uint32_t h) {
    const DXGI_FORMAT fmt = pick_format(src_format);
    if (fmt == DXGI_FORMAT_UNKNOWN) {
        // Asked every frame; said once per game format (09 §7).
        if (sc.results.no_format != src_format) {
            sc.results.no_format = src_format;
            diag::logf("openxr: runtime has no swapchain format compatible with {}; no layer will be submitted",
                       static_cast<int>(src_format));
        }
        return false;
    }
    if (sc.handle != XR_NULL_HANDLE && sc.format == fmt && sc.width == w && sc.height == h) return true;
    destroy_chain(sc);

    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    ci.format      = fmt;
    ci.sampleCount = 1;
    ci.width       = w;
    ci.height      = h;
    ci.faceCount   = 1;
    ci.arraySize   = 1;
    ci.mipCount    = 1;
    // Retried every frame while it fails: logged on change only.
    if (!check_frame(xrCreateSwapchain(g.session, &ci, &sc.handle), "xrCreateSwapchain", sc.results.create)) {
        sc.handle = XR_NULL_HANDLE;
        return false;
    }

    std::uint32_t n = 0;
    xrEnumerateSwapchainImages(sc.handle, 0, &n, nullptr);
    sc.images.assign(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    if (!check_frame(xrEnumerateSwapchainImages(sc.handle, n, &n,
                         reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data())),
                     "xrEnumerateSwapchainImages", sc.results.images)) {
        destroy_chain(sc);
        return false;
    }
    sc.width = w;
    sc.height = h;
    sc.format = fmt;
    diag::logf("openxr: swapchain {}x{} fmt={} images={}", w, h, static_cast<int>(fmt), n);
    return true;
}

// Copy `src` (region `box`) into the next image of `sc`.
bool Session::copy_into(Swapchain& sc, ID3D11Texture2D* src, D3D11_BOX box) {
    box.right  = (std::min)(box.right,  box.left + sc.width);
    box.bottom = (std::min)(box.bottom, box.top + sc.height);
    box.front = 0;
    box.back  = 1;

    std::uint32_t index = 0;
    XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if (!check_frame(xrAcquireSwapchainImage(sc.handle, &ai, &index), "xrAcquireSwapchainImage",
                     sc.results.acquire)) {
        return false;
    }
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wi.timeout = XR_INFINITE_DURATION;
    const XrResult wr = xrWaitSwapchainImage(sc.handle, &wi);
    // A timeout is a success code but leaves the image unwritten.
    const bool waited = check_frame(wr, "xrWaitSwapchainImage", sc.results.wait) && wr != XR_TIMEOUT_EXPIRED;
    if (waited) g.context->CopySubresourceRegion(sc.images[index].texture, 0, 0, 0, 0, src, 0, &box);
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const bool released = check_frame(xrReleaseSwapchainImage(sc.handle, &ri), "xrReleaseSwapchainImage",
                                      sc.results.release);
    // An image that was not written must not be shown: the frame then ends
    // with no layer (09 §5.2).
    return waited && released;
}

} // namespace omvr::platform::openxr
