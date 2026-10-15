#pragma once

// Owners of OpenXR handles (STYLEGUIDE 5): each destroys its handle when
// reset or destroyed, so no xrDestroy* call appears outside them. Where the
// order of destruction matters (09 §3: swapchains, input, spaces, session,
// instance) the owner's caller resets them explicitly in that order.

#include "platform/openxr/xr_headers.h"

#include <utility>

namespace omvr::platform::openxr {

template <class Handle, auto Destroy>
class XrOwner {
public:
    XrOwner() noexcept = default;
    explicit XrOwner(Handle h) noexcept : h_(h) {}
    ~XrOwner() { reset(); }
    XrOwner(XrOwner&& o) noexcept : h_(std::exchange(o.h_, Handle{XR_NULL_HANDLE})) {}
    XrOwner& operator=(XrOwner&& o) noexcept {
        if (this != &o) reset(std::exchange(o.h_, Handle{XR_NULL_HANDLE}));
        return *this;
    }
    XrOwner(const XrOwner&)            = delete;
    XrOwner& operator=(const XrOwner&) = delete;

    Handle get() const noexcept { return h_; }
    explicit operator bool() const noexcept { return h_ != XR_NULL_HANDLE; }

    // Destroys the handle held, if any, and holds `h`.
    void reset(Handle h = XR_NULL_HANDLE) noexcept {
        if (h_ != XR_NULL_HANDLE) Destroy(h_);
        h_ = h;
    }
    // Forgets the handle without destroying it: what a failed create call
    // left behind is not a handle.
    void release() noexcept { h_ = XR_NULL_HANDLE; }
    // Destroys the handle held, then gives the create call somewhere to put
    // the new one.
    Handle* put() noexcept {
        reset();
        return &h_;
    }

private:
    Handle h_ = XR_NULL_HANDLE;
};

using InstanceOwner  = XrOwner<XrInstance, xrDestroyInstance>;
using SessionOwner   = XrOwner<XrSession, xrDestroySession>;
using SpaceOwner     = XrOwner<XrSpace, xrDestroySpace>;
using SwapchainOwner = XrOwner<XrSwapchain, xrDestroySwapchain>;
using ActionSetOwner = XrOwner<XrActionSet, xrDestroyActionSet>;

} // namespace omvr::platform::openxr
