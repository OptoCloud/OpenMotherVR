#include "platform/openxr/session.h"

#include "core/diag/logger.h" // R: vr (A1): log through an injected Log&
#include "core/ui/notices.h"
#include "core/vr/tuning.h"
#include "platform/openxr/result_names.h"
#include "platform/win32/text.h"
#include "util/math/angle.h"

#include <chrono>
#include <cmath>
#include <string>

namespace omvr::platform::openxr {

namespace {

namespace diag {
using core::diag::log_line;
using core::diag::logf;
using win32::utf8;
} // namespace diag
using core::vr::kVisibleTimeout;

constexpr XrPosef kIdentity{{0, 0, 0, 1}, {0, 0, 0}};

} // namespace

bool all_finite(const Pose& p) noexcept {
    const float v[7] = {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w,
                        p.position.x, p.position.y, p.position.z};
    for (float f : v)
        if (!std::isfinite(f)) return false;
    return true;
}

// `space` in `base` at `t`, and its location flags; flags 0 when the call
// fails or the pose is not finite, so nothing NaN is ever handed on (09 §5.5).
XrSpaceLocationFlags locate_flags(XrSpace space, XrSpace base, XrTime t, Pose& out) {
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(space, base, t, &loc))) return 0;
    const Pose p = from_xr(loc.pose);
    if (!all_finite(p)) return 0;
    out = p;
    return loc.locationFlags;
}

// Both orientation and position valid: what a re-anchor needs.
bool locate(XrSpace space, XrSpace base, XrTime t, Pose& out) {
    Pose p{};
    if ((locate_flags(space, base, t, p) & kOrientationAndPosition) != kOrientationAndPosition) return false;
    out = p;
    return true;
}

XrSpace Session::make_reference_space(XrReferenceSpaceType type, const XrPosef& pose) {
    XrReferenceSpaceCreateInfo ci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    ci.referenceSpaceType   = type;
    ci.poseInReferenceSpace = pose;
    XrSpace s = XR_NULL_HANDLE;
    check(xrCreateReferenceSpace(g.session, &ci, &s), "xrCreateReferenceSpace");
    return s;
}

// Seated origin at the head's current position, facing its yaw (pitch and
// roll ignored so the world stays level).
bool Session::do_recenter(XrTime t) {
    Pose head{};
    if (!locate(g.view, g.local, t, head)) return false;   // stays pending (09 §6.3)
    const Quat& q = head.orientation;
    // Looking straight up or down leaves no heading: keep the previous
    // anchor's yaw with the new position, as a game recenter does (09 §5.5).
    const float fx = 2.0f * (q.w * q.y + q.x * q.z), fz = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    const bool vertical = std::sqrt(fx * fx + fz * fz) < 1e-4f;
    const float yaw = vertical ? g.anchor_yaw : std::atan2(fx, fz);
    const XrPosef origin{{0, std::sin(yaw * 0.5f), 0, std::cos(yaw * 0.5f)},
                         {head.position.x, head.position.y, head.position.z}};
    XrSpace s = make_reference_space(XR_REFERENCE_SPACE_TYPE_LOCAL, origin);
    if (s == XR_NULL_HANDLE) return false;
    if (g.seated != XR_NULL_HANDLE) xrDestroySpace(g.seated);
    g.seated = s;
    g.anchor_yaw = yaw;
    diag::logf("openxr: recentered (yaw {:.1f} deg{})", yaw * 57.29578f,
               vertical ? ", kept: looking straight up or down" : "");
    return true;
}

void Session::check_visible() noexcept {
    if (g_visible_once || g_running_since == std::chrono::steady_clock::time_point{}) return;
    if (g.state == XR_SESSION_STATE_VISIBLE || g.state == XR_SESSION_STATE_FOCUSED) {
        g_visible_once = true;
        return;
    }
    if (std::chrono::steady_clock::now() - g_running_since < kVisibleTimeout) return;
    g_visible_once = true;  // said once
    diag::logf("openxr: session not visible after {} s (state {})", kVisibleTimeout.count(), state_name(g.state));
    dialogs_.show_async(std::string(core::ui::openxr_not_showing_game()), core::ui::ports::DialogKind::Warning,
                        "headset not showing the game");
}

void Session::handle_state(XrSessionState s) {
    g.state = s;
    diag::logf("openxr: session state {}", state_name(s));
    switch (s) {
        case XR_SESSION_STATE_READY: {
            XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
            bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            g.running = check(xrBeginSession(g.session, &bi), "xrBeginSession");
            if (g.running) {
                diag::log_line("openxr: session running");
                g_running_since = std::chrono::steady_clock::now();
            }
            break;
        }
        case XR_SESSION_STATE_STOPPING:
            g.running = false;
            check(xrEndSession(g.session), "xrEndSession");
            diag::log_line("openxr: session stopped");
            break;
        case XR_SESSION_STATE_EXITING:
            g.exit_requested = true; // the player quit from the runtime's menu
            g.running = false;
            g.lost = true;
            diag::log_line("openxr: runtime requested exit; game asked to quit, VR off until restart");
            break;
        case XR_SESSION_STATE_LOSS_PENDING:
            // Not a request to quit: the game keeps running on the mirror (09 §3, §7).
            g.running = false;
            g.lost = true;
            diag::log_line("openxr: session loss pending; VR off until restart, the game continues");
            break;
        default:
            break;
    }
}

void Session::poll_events() {
    for (;;) {
        XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
        if (xrPollEvent(g.instance, &ev) != XR_SUCCESS) break;
        switch (ev.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                handle_state(reinterpret_cast<const XrEventDataSessionStateChanged&>(ev).state);
                break;
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                g.running = false;
                g.lost = true;
                diag::log_line("openxr: instance loss pending");
                break;
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                input.on_interaction_profile_changed();
                break;
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                g.recenter_pending = true;
                break;
            default:
                break;
        }
    }
}

void Session::teardown() {
    for (auto& sc : g.eye_chain) destroy_chain(sc);
    destroy_chain(g.screen_chain);
    input.destroy();
    for (XrSpace* s : {&g.seated, &g.local, &g.view}) {
        if (*s != XR_NULL_HANDLE) xrDestroySpace(*s);
        *s = XR_NULL_HANDLE;
    }
    if (g.session != XR_NULL_HANDLE) xrDestroySession(g.session);
    if (g.instance != XR_NULL_HANDLE) xrDestroyInstance(g.instance);
    if (g.context) g.context->Release();
    if (g.device) g.device->Release();
    g = State{};
    g_views.store({});
}

session_info Session::init(ID3D11Device* device) noexcept {
    session_info r{};
    if (!device || !init_system()) return r;

    // Required before xrCreateSession. The runtime must composite on the
    // adapter the game renders with; warn if they differ (hybrid laptops).
    PFN_xrGetD3D11GraphicsRequirementsKHR get_reqs = nullptr;
    xrGetInstanceProcAddr(g.instance, "xrGetD3D11GraphicsRequirementsKHR",
                          reinterpret_cast<PFN_xrVoidFunction*>(&get_reqs));
    XrGraphicsRequirementsD3D11KHR reqs{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    if (!get_reqs || !check(get_reqs(g.instance, g.system, &reqs), "xrGetD3D11GraphicsRequirementsKHR")) {
        teardown();
        return r;
    }
    IDXGIDevice* dxgi = nullptr;
    if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi)))) {
        IDXGIAdapter* adapter = nullptr;
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(dxgi->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetDesc(&desc)) &&
            (desc.AdapterLuid.LowPart != reqs.adapterLuid.LowPart ||
             desc.AdapterLuid.HighPart != reqs.adapterLuid.HighPart)) {
            // Both adapters, so a hybrid-GPU report can be read from the log (11 §2.4).
            diag::logf("openxr: WARNING game renders on {} (LUID {:08X}:{:08X}), the runtime wants adapter "
                       "LUID {:08X}:{:08X}; continuing",
                       diag::utf8(std::filesystem::path{desc.Description}),
                       static_cast<std::uint32_t>(desc.AdapterLuid.HighPart), desc.AdapterLuid.LowPart,
                       static_cast<std::uint32_t>(reqs.adapterLuid.HighPart), reqs.adapterLuid.LowPart);
        }
        if (adapter) adapter->Release();
        dxgi->Release();
    }

    g.device = device;
    g.device->AddRef();
    g.device->GetImmediateContext(&g.context);

    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device = device;
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next     = &binding;
    sci.systemId = g.system;
    if (!check(xrCreateSession(g.instance, &sci, &g.session), "xrCreateSession")) {
        teardown();
        return r;
    }

    g.local  = make_reference_space(XR_REFERENCE_SPACE_TYPE_LOCAL, kIdentity);
    g.seated = make_reference_space(XR_REFERENCE_SPACE_TYPE_LOCAL, kIdentity);
    g.view   = make_reference_space(XR_REFERENCE_SPACE_TYPE_VIEW, kIdentity);
    if (g.local == XR_NULL_HANDLE || g.seated == XR_NULL_HANDLE || g.view == XR_NULL_HANDLE) {
        // Nothing could be located or submitted without them.
        teardown();
        return r;
    }

    std::uint32_t n = 0;
    xrEnumerateEnvironmentBlendModes(g.instance, g.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 1, &n, &g.blend);

    xrEnumerateSwapchainFormats(g.session, 0, &n, nullptr);
    g.formats.resize(n);
    xrEnumerateSwapchainFormats(g.session, n, &n, g.formats.data());

    if (!input.create(g.instance, g.session)) {
        diag::log_line("openxr: controller input unavailable");
    }

    r.ok            = true;
    r.recommended_w = g.config_views[0].recommendedImageRectWidth;
    r.recommended_h = g.config_views[0].recommendedImageRectHeight;
    diag::logf("openxr: session created, per-eye {}x{}", r.recommended_w, r.recommended_h);
    return r;
}

void Session::shutdown() noexcept {
    if (g.session != XR_NULL_HANDLE && g.running) xrEndSession(g.session);
    teardown();
}

bool Session::running() const noexcept { return g.running && !g.lost; }

bool Session::exit_requested() const noexcept { return g.exit_requested; }

FrameViews Session::views() const noexcept { return g_views.load(); }

void Session::set_eye_images(const EyeImages& images) noexcept {
    g.eye_images = images;
    g.eye_images_set = true;
}

void Session::clear_eye_images() noexcept { g.eye_images_set = false; }

void Session::set_screen_image(ID3D11Texture2D* texture, const D3D11_BOX& region) noexcept {
    g.screen_image  = texture;
    g.screen_region = region;
}

void Session::set_screen_pose(const Pose& pose, float width_m) noexcept {
    g.screen_pose     = pose;
    g.screen_width    = width_m;
    g.screen_pose_set = true;
}

void Session::recenter() noexcept { g.recenter_pending = true; }

} // namespace omvr::platform::openxr
