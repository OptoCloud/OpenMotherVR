// OpenMotherVR-Provenance: clean
#include "platform/openxr/session.h"

#include "core/ui/notices.h"
#include "core/vr/heading.h"
#include "core/vr/runtime_failure.h"
#include "core/vr/tuning.h"
#include "platform/d3d11/handles.h"
#include "platform/openxr/result_names.h"
#include "platform/openxr/session_impl.h"
#include "platform/win32/text.h"
#include "util/math/angle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <string>
#include <string_view>
#include <vector>

namespace omvr::platform::openxr {

namespace {

using core::diag::ports::logf;
using core::vr::kVisibleTimeout;

constexpr std::string_view kLogRecentered = "openxr: recentered (yaw {:.1f} deg{})";
constexpr std::string_view kLogNotVisible = "openxr: session not visible after {} s (state {})";
constexpr std::string_view kLogState      = "openxr: session state {}";
constexpr std::string_view kLogRunning    = "openxr: session running";
constexpr std::string_view kLogStopped    = "openxr: session stopped";
constexpr std::string_view kLogExitRequested =
    "openxr: runtime requested exit; game asked to quit, VR off until restart";
constexpr std::string_view kLogLossPending =
    "openxr: session loss pending; VR off until restart, the game continues";
constexpr std::string_view kLogInstanceLoss = "openxr: instance loss pending";
constexpr std::string_view kLogAdapterMismatch =
    "openxr: WARNING game renders on {} (LUID {:08X}:{:08X}), the runtime wants adapter LUID {:08X}:{:08X}; "
    "continuing";
constexpr std::string_view kLogNoInput     = "openxr: controller input unavailable";
constexpr std::string_view kLogCreated     = "openxr: session created, per-eye {}x{}";
constexpr std::string_view kLogExitRequest = "openxr: ending the session at the player's request";
constexpr std::string_view kLogExitTimeout =
    "openxr: the runtime did not stop the session within {} ms; destroying it running";
constexpr std::string_view kLogEnded   = "openxr: session and instance destroyed; VR off until restart";
constexpr std::string_view kLogFloor   = "openxr: floor reference {}";
constexpr std::string_view kLogNoFloor = "openxr: no STAGE or LOCAL_FLOOR space: standing play unavailable";

// The log label of the headset-not-showing-the-game warning (09 §3.1).
constexpr std::string_view kNotShowingLabel = "headset not showing the game";

bool same_adapter(const LUID& a, const LUID& b) noexcept {
    return a.LowPart == b.LowPart && a.HighPart == b.HighPart;
}

} // namespace

// ---- Locating spaces -----------------------------------------------------------

bool all_finite(const Pose& p) noexcept {
    const float v[] = {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w,
                       p.position.x,    p.position.y,    p.position.z};
    for (float f : v)
        if (!std::isfinite(f)) return false;
    return true;
}

XrSpaceLocationFlags locate_flags(XrSpace space, XrSpace base, XrTime t, Pose& out) {
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(space, base, t, &loc))) return 0;
    const Pose p = from_xr(loc.pose);
    if (!all_finite(p)) return 0;
    out = p;
    return loc.locationFlags;
}

bool locate(XrSpace space, XrSpace base, XrTime t, Pose& out) {
    Pose p{};
    if ((locate_flags(space, base, t, p) & kOrientationAndPosition) != kOrientationAndPosition) return false;
    out = p;
    return true;
}

// ---- Spaces and recentering ------------------------------------------------------

XrSpace Session::Impl::make_reference_space(XrReferenceSpaceType type, const XrPosef& pose) {
    XrReferenceSpaceCreateInfo ci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    ci.referenceSpaceType   = type;
    ci.poseInReferenceSpace = pose;
    XrSpace s               = XR_NULL_HANDLE;
    check(xrCreateReferenceSpace(state.session.get(), &ci, &s), "xrCreateReferenceSpace");
    return s;
}

// Seated origin at the head's current position, facing its yaw (pitch and
// roll ignored so the world stays level; 09 §6.3).
bool Session::Impl::do_recenter(XrTime t) {
    Pose head{};
    if (!locate(state.view.get(), state.local.get(), t, head)) return false; // stays pending (09 §6.3)
    // Looking straight up or down leaves no heading: keep the previous
    // anchor's yaw with the new position, as a game recenter does (09 §5.5).
    const core::vr::Heading heading = core::vr::heading_of(head.orientation, state.anchor_yaw);
    const XrPosef origin            = to_xr(Pose{core::vr::yaw_rotation(heading.yaw_rad), head.position});
    const XrSpace s                 = make_reference_space(XR_REFERENCE_SPACE_TYPE_LOCAL, origin);
    if (s == XR_NULL_HANDLE) return false;
    state.seated.reset(s);
    state.anchor_yaw = heading.yaw_rad;
    logf(log, kLogRecentered, heading.yaw_rad * util::kRadToDeg,
         heading.vertical ? ", kept: looking straight up or down" : "");
    return true;
}

// ---- Session state ---------------------------------------------------------------

void Session::Impl::check_visible() noexcept {
    if (visible_reported || running_since == std::chrono::steady_clock::time_point{}) return;
    if (state.session_state == XR_SESSION_STATE_VISIBLE || state.session_state == XR_SESSION_STATE_FOCUSED) {
        visible_reported = true;
        return;
    }
    if (clock.now() - running_since < kVisibleTimeout) return;
    visible_reported = true; // said once
    logf(log, kLogNotVisible, kVisibleTimeout.count(), state_name(state.session_state));
    dialogs.show_async(std::string(core::ui::openxr_not_showing_game()), core::ui::ports::DialogKind::Warning,
                       kNotShowingLabel);
}

void Session::Impl::handle_state(XrSessionState s) {
    state.session_state = s;
    logf(log, kLogState, state_name(s));
    switch (s) {
        case XR_SESSION_STATE_READY: {
            XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
            bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            state.running = check(xrBeginSession(state.session.get(), &bi), "xrBeginSession");
            if (state.running) {
                log.line(kLogRunning);
                running_since = clock.now();
            }
            break;
        }
        case XR_SESSION_STATE_STOPPING:
            state.running = false;
            check(xrEndSession(state.session.get()), "xrEndSession");
            log.line(kLogStopped);
            break;
        case XR_SESSION_STATE_EXITING:
            state.exit_requested = true; // the player quit from the runtime's menu
            state.running        = false;
            state.lost           = true;
            log.line(kLogExitRequested);
            break;
        case XR_SESSION_STATE_LOSS_PENDING:
            // Not a request to quit: the game keeps running on the mirror (09 §3, §7).
            state.running = false;
            state.lost    = true;
            log.line(kLogLossPending);
            break;
        default: break;
    }
}

void Session::Impl::poll_events() {
    for (;;) {
        XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
        if (xrPollEvent(state.instance.get(), &ev) != XR_SUCCESS) break;
        switch (ev.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                handle_state(reinterpret_cast<const XrEventDataSessionStateChanged&>(ev).state);
                break;
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                state.running = false;
                state.lost    = true;
                log.line(kLogInstanceLoss);
                break;
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                input.on_interaction_profile_changed();
                break;
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: state.recenter_pending = true; break;
            default: break;
        }
    }
}

// In the order 09 §3 needs: swapchains, input, spaces, session, instance,
// then the game's device.
void Session::Impl::teardown() {
    for (auto& sc : state.eye_chain) destroy_chain(sc);
    destroy_chain(state.screen_chain);
    input.destroy();
    state.seated.reset();
    state.local.reset();
    state.view.reset();
    state.floor.reset();
    floor_available.store(false, std::memory_order_relaxed);
    state.session.reset();
    state.instance.reset();
    state.context.reset();
    state.device.reset();
    state = State{};
    published_views.store({});
}

// ---- Creation --------------------------------------------------------------------

// The runtime must composite on the adapter the game renders with; warn if
// they differ (hybrid laptops), naming both so a report can be read from the
// log (11 §2.4).
void Session::Impl::warn_adapter_mismatch(ID3D11Device* device, const XrGraphicsRequirementsD3D11KHR& reqs) {
    ComPtr<IDXGIDevice> dxgi;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(dxgi.put())))) return;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC desc{};
    if (SUCCEEDED(dxgi->GetAdapter(adapter.put())) && SUCCEEDED(adapter->GetDesc(&desc)) &&
        !same_adapter(desc.AdapterLuid, reqs.adapterLuid)) {
        logf(log, kLogAdapterMismatch, win32::utf8(std::filesystem::path{desc.Description}),
             static_cast<std::uint32_t>(desc.AdapterLuid.HighPart), desc.AdapterLuid.LowPart,
             static_cast<std::uint32_t>(reqs.adapterLuid.HighPart), reqs.adapterLuid.LowPart);
    }
}

bool Session::Impl::create_session(ID3D11Device* device) {
    state.device = ComPtr<ID3D11Device>::share(device); // a reference of our own
    device->GetImmediateContext(state.context.put());

    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device = device;
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next     = &binding;
    sci.systemId = state.system;
    return check(xrCreateSession(state.instance.get(), &sci, state.session.put()), "xrCreateSession");
}

// Nothing can be located or submitted without the three spaces.
bool Session::Impl::create_spaces() {
    state.local.reset(make_reference_space(XR_REFERENCE_SPACE_TYPE_LOCAL, kIdentityPose));
    state.seated.reset(make_reference_space(XR_REFERENCE_SPACE_TYPE_LOCAL, kIdentityPose));
    state.view.reset(make_reference_space(XR_REFERENCE_SPACE_TYPE_VIEW, kIdentityPose));
    if (!(state.local && state.seated && state.view)) return false;
    create_floor_space();
    return true;
}

// 22 §6.1: the floor for standing play, STAGE when the runtime has it, else
// LOCAL_FLOOR (XR_EXT_local_floor). Optional: without either, play is
// seated.
void Session::Impl::create_floor_space() {
    std::uint32_t n = 0;
    xrEnumerateReferenceSpaces(state.session.get(), 0, &n, nullptr);
    std::vector<XrReferenceSpaceType> types(n);
    if (n == 0 || XR_FAILED(xrEnumerateReferenceSpaces(state.session.get(), n, &n, types.data())))
        types.clear();
    const auto has = [&types](XrReferenceSpaceType t) { return std::ranges::find(types, t) != types.end(); };
    XrReferenceSpaceType type = XR_REFERENCE_SPACE_TYPE_STAGE;
    const char* name          = "STAGE";
    if (!has(type)) {
        type = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
        name = "LOCAL_FLOOR";
    }
    if (has(type)) state.floor.reset(make_reference_space(type, kIdentityPose));
    floor_available.store(static_cast<bool>(state.floor), std::memory_order_relaxed);
    if (state.floor) {
        logf(log, kLogFloor, name);
    } else {
        log.line(kLogNoFloor);
    }
}

SessionInfo Session::Impl::init(ID3D11Device* device) noexcept {
    SessionInfo r{};
    if (!device || !init_system()) return r;

    // Required before xrCreateSession (XR_KHR_D3D11_enable).
    PFN_xrGetD3D11GraphicsRequirementsKHR get_reqs = nullptr;
    xrGetInstanceProcAddr(state.instance.get(), "xrGetD3D11GraphicsRequirementsKHR",
                          reinterpret_cast<PFN_xrVoidFunction*>(&get_reqs));
    XrGraphicsRequirementsD3D11KHR reqs{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    if (!get_reqs ||
        !check(get_reqs(state.instance.get(), state.system, &reqs), "xrGetD3D11GraphicsRequirementsKHR")) {
        teardown();
        return r;
    }
    warn_adapter_mismatch(device, reqs);
    if (!create_session(device) || !create_spaces()) {
        teardown();
        return r;
    }

    std::uint32_t n = 0;
    xrEnumerateEnvironmentBlendModes(state.instance.get(), state.system,
                                     XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 1, &n, &state.blend);
    xrEnumerateSwapchainFormats(state.session.get(), 0, &n, nullptr);
    state.formats.resize(n);
    xrEnumerateSwapchainFormats(state.session.get(), n, &n, state.formats.data());

    if (!input.create(state.instance.get(), state.session.get())) log.line(kLogNoInput);

    r.ok            = true;
    r.recommended_w = state.config_views[0].recommendedImageRectWidth;
    r.recommended_h = state.config_views[0].recommendedImageRectHeight;
    logf(log, kLogCreated, r.recommended_w, r.recommended_h);
    return r;
}

void Session::Impl::shutdown() noexcept {
    if (state.session && state.running) xrEndSession(state.session.get());
    teardown();
}

// Polls events until handle_state has seen STOPPING (and called
// xrEndSession) or the session is lost; false when the wait ran out.
bool Session::Impl::wait_for_stopping() {
    const auto deadline = clock.now() + core::vr::kExitSessionWait;
    for (;;) {
        poll_events();
        if (!state.running || state.lost) return true;
        if (clock.now() >= deadline) return false;
        std::this_thread::sleep_for(core::vr::kExitSessionPoll);
    }
}

// 22 §13: xrRequestExitSession, xrEndSession on STOPPING, then the handles
// in the order teardown keeps (the session before the instance).
void Session::Impl::end_on_request() noexcept {
    try {
        if (state.session && state.running) {
            log.line(kLogExitRequest);
            if (check(xrRequestExitSession(state.session.get()), "xrRequestExitSession") &&
                !wait_for_stopping())
                logf(log, kLogExitTimeout, core::vr::kExitSessionWait.count());
        }
        teardown();
        log.line(kLogEnded);
    } catch (...) {
        // Logging or event handling threw; the handles go regardless.
        teardown();
    }
}

// ---- Session ---------------------------------------------------------------------

Session::Session(core::diag::ports::Log& log, const core::diag::ports::Clock& clock,
                 core::ui::ports::Dialogs& dialogs, core::vr::RuntimeFailure& failure)
    : impl_(std::make_unique<Impl>(log, clock, dialogs, failure)) {}

Session::~Session() = default;

std::filesystem::path Session::probe() noexcept { return impl_->probe(); }

SessionInfo Session::init(core::vr::GpuDevice device) noexcept { return impl_->init(d3d11::to_d3d(device)); }

bool Session::init_system() noexcept { return impl_->init_system(); }

const SystemInfo& Session::system_info() const noexcept { return impl_->state.info; }

void Session::shutdown() noexcept { impl_->shutdown(); }

void Session::end_on_request() noexcept { impl_->end_on_request(); }

void Session::set_aim_poses(bool on) noexcept { impl_->input.set_aim_poses(on); }

void Session::set_haptics(bool on) noexcept { impl_->input.set_haptics(on); }

bool Session::running() const noexcept { return impl_->state.running && !impl_->state.lost; }

bool Session::has_floor() const noexcept { return impl_->floor_available.load(std::memory_order_relaxed); }

bool Session::exit_requested() const noexcept { return impl_->state.exit_requested; }

FrameViews Session::views() const noexcept { return impl_->published_views.load(); }

void Session::set_eye_images(const EyeImages& images) noexcept {
    impl_->state.eye_images     = images;
    impl_->state.eye_images_set = true;
}

void Session::clear_eye_images() noexcept { impl_->state.eye_images_set = false; }

bool Session::frame(core::stereo::SwapchainHandle swapchain) noexcept {
    return impl_->frame(d3d11::to_d3d(swapchain));
}

void Session::recenter() noexcept { impl_->state.recenter_pending = true; }

void Session::set_screen_image(core::vr::GpuTexture texture, const TextureRegion& region) noexcept {
    impl_->state.screen_image  = d3d11::to_d3d(texture);
    impl_->state.screen_region = region;
}

void Session::set_screen_pose(const util::Pose& pose, float width_m) noexcept {
    impl_->state.screen_pose     = pose;
    impl_->state.screen_width    = width_m;
    impl_->state.screen_pose_set = true;
}

const ControllerState& Session::controllers() const noexcept { return impl_->input.state(); }

void Session::play_haptics(const core::vr::ports::HapticFrame& frame) noexcept {
    impl_->input.play_haptics(frame);
}

} // namespace omvr::platform::openxr
