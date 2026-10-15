#include "platform/openxr/swapchain.h"

#include "platform/openxr/session_impl.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace omvr::platform::openxr {

namespace {

using core::diag::ports::logf;

constexpr std::string_view kLogNoFormat =
    "openxr: runtime has no swapchain format compatible with {}; no layer will be submitted";
constexpr std::string_view kLogCreated = "openxr: swapchain {}x{} fmt={} images={}";

// The formats a source of format `src` can be copied into, best first
// (CopySubresourceRegion needs the same typeless family). sRGB variants
// come first: the game's back buffer holds gamma-encoded values, and an sRGB
// swapchain tells the compositor exactly that (09 §5.2). Asked every frame,
// so no allocation (12 §6.1).
struct WantedFormats {
    std::array<DXGI_FORMAT, 2> formats{};
    std::size_t                count = 0;
};

WantedFormats wanted_formats(DXGI_FORMAT src) noexcept {
    switch (src) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return {{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM}, 2};
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return {{DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM}, 2};
        default:
            return {{src}, 1};
    }
}

} // namespace

// The first wanted format the runtime offers, or UNKNOWN.
DXGI_FORMAT Session::Impl::pick_format(DXGI_FORMAT src) {
    const WantedFormats wanted = wanted_formats(src);
    for (std::size_t i = 0; i < wanted.count; ++i) {
        const DXGI_FORMAT f = wanted.formats[i];
        for (std::int64_t have : state.formats) {
            if (have == static_cast<std::int64_t>(f)) return f;
        }
    }
    return DXGI_FORMAT_UNKNOWN;
}

void Session::Impl::destroy_chain(Swapchain& sc) {
    sc.handle.reset();
    const ChainResults results = sc.results;
    sc = {};
    sc.results = results;
}

// A single-sample colour chain the game's images are copied into. Retried
// every frame while it fails: logged on change only.
bool Session::Impl::create_chain(Swapchain& sc, DXGI_FORMAT format, std::uint32_t w, std::uint32_t h) {
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags  = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    ci.format      = format;
    ci.sampleCount = 1;
    ci.width       = w;
    ci.height      = h;
    ci.faceCount   = 1;
    ci.arraySize   = 1;
    ci.mipCount    = 1;
    if (!check_frame(xrCreateSwapchain(state.session.get(), &ci, sc.handle.put()), "xrCreateSwapchain",
                     sc.results.create)) {
        sc.handle.release();   // nothing was created
        return false;
    }

    std::uint32_t n = 0;
    xrEnumerateSwapchainImages(sc.handle.get(), 0, &n, nullptr);
    sc.images.assign(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    if (!check_frame(xrEnumerateSwapchainImages(sc.handle.get(), n, &n,
                                                reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data())),
                     "xrEnumerateSwapchainImages", sc.results.images)) {
        destroy_chain(sc);
        return false;
    }
    sc.width  = w;
    sc.height = h;
    sc.format = format;
    logf(log, kLogCreated, w, h, static_cast<int>(format), n);
    return true;
}

bool Session::Impl::ensure_chain(Swapchain& sc, DXGI_FORMAT src_format, std::uint32_t w, std::uint32_t h) {
    const DXGI_FORMAT fmt = pick_format(src_format);
    if (fmt == DXGI_FORMAT_UNKNOWN) {
        // Asked every frame; said once per game format (09 §7).
        if (sc.results.no_format != src_format) {
            sc.results.no_format = src_format;
            logf(log, kLogNoFormat, static_cast<int>(src_format));
        }
        return false;
    }
    if (sc.handle && sc.format == fmt && sc.width == w && sc.height == h) return true;
    destroy_chain(sc);
    return create_chain(sc, fmt, w, h);
}

// Copy `src` (region `box`) into the next image of `sc`.
bool Session::Impl::copy_into(Swapchain& sc, ID3D11Texture2D* src, D3D11_BOX box) {
    box.right  = (std::min)(box.right,  box.left + sc.width);
    box.bottom = (std::min)(box.bottom, box.top + sc.height);
    box.front = 0;
    box.back  = 1;

    std::uint32_t index = 0;
    XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if (!check_frame(xrAcquireSwapchainImage(sc.handle.get(), &ai, &index), "xrAcquireSwapchainImage",
                     sc.results.acquire)) {
        return false;
    }
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wi.timeout = XR_INFINITE_DURATION;
    const XrResult wr = xrWaitSwapchainImage(sc.handle.get(), &wi);
    // A timeout is a success code but leaves the image unwritten.
    const bool waited = check_frame(wr, "xrWaitSwapchainImage", sc.results.wait) && wr != XR_TIMEOUT_EXPIRED;
    if (waited) state.context->CopySubresourceRegion(sc.images[index].texture, 0, 0, 0, 0, src, 0, &box);
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const bool released = check_frame(xrReleaseSwapchainImage(sc.handle.get(), &ri), "xrReleaseSwapchainImage",
                                      sc.results.release);
    // An image that was not written must not be shown: the frame then ends
    // with no layer (09 §5.2).
    return waited && released;
}

} // namespace omvr::platform::openxr
