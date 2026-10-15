// The OpenXR frame boundary (09 §5): end the frame the previous call began,
// then begin the next one. Render thread.

#include "core/vr/pose_hold.h"
#include "core/vr/tuning.h"
#include "platform/d3d11/handles.h"
#include "platform/openxr/session_impl.h"

#include <cmath>
#include <string_view>

namespace omvr::platform::openxr {

namespace {

using core::diag::ports::logf;
using core::vr::kDefaultScreenDistance;
using core::vr::kDefaultScreenWidth;

constexpr std::string_view kLogPoseAge =
    "openxr: pose age over {} frames: {} same frame, {} one period early, {} other, mean {:.2f} periods over {} known";
constexpr std::string_view kLogIpd = "openxr: IPD {:.4f} m";

D3D11_BOX to_box(const TextureRegion& r) noexcept { return {r.left, r.top, 0, r.right, r.bottom, 1}; }

} // namespace

// The layers end_frame may submit, alive until xrEndFrame returns.
struct Session::Impl::FrameLayers {
    XrCompositionLayerProjection proj{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::array<XrCompositionLayerProjectionView, core::vr::kEyeCount> views{
        {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
    XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
};

// Both eye images as a projection layer, declared with the pose and FOV
// each was rendered with; null when an image is missing or could not be
// copied (09 §5.2).
const XrCompositionLayerBaseHeader* Session::Impl::projection_layer(FrameLayers& layers) {
    const EyeImages& images = state.eye_images;
    bool ok = true;
    for (std::size_t i = 0; i < core::vr::kEyeCount && ok; ++i) {
        ID3D11Texture2D* tex = d3d11::to_d3d(images.texture[i]);
        if (!tex) {
            ok = false;
            break;
        }
        D3D11_TEXTURE2D_DESC d{};
        tex->GetDesc(&d);
        const D3D11_BOX box = to_box(images.region[i]);
        Swapchain& chain = state.eye_chain[i];
        ok = ensure_chain(chain, d.Format, box.right - box.left, box.bottom - box.top) && copy_into(chain, tex, box);
        auto& view = layers.views[i];
        view.pose = to_xr(images.view[i].pose);
        view.fov  = to_xr(images.view[i].fov);
        view.subImage.swapchain = chain.handle.get();
        view.subImage.imageRect = {{0, 0}, {static_cast<std::int32_t>(chain.width), static_cast<std::int32_t>(chain.height)}};
    }
    if (!ok) return nullptr;
    layers.proj.space     = state.seated.get();
    layers.proj.viewCount = static_cast<std::uint32_t>(layers.views.size());
    layers.proj.views     = layers.views.data();
    if (auto report = state.pose_age.add(state.frame_time, images.rendered_for, state.frame_period);
        report && log.debug_enabled()) {
        // The mean is over the frames whose age is known (11 §4.1).
        logf(log, kLogPoseAge, report->frames, report->same, report->one_early, report->other, report->mean,
             report->known);
    }
    return reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layers.proj);
}

// Virtual screen: a region of the given image, or else the finished back
// buffer, on a world-locked quad (09 §5.2, §8). Null when there is nothing
// to show or it could not be copied.
const XrCompositionLayerBaseHeader* Session::Impl::screen_layer(FrameLayers& layers, IDXGISwapChain* swapchain) {
    ID3D11Texture2D* src = state.screen_image;
    D3D11_BOX box = to_box(state.screen_region);
    ComPtr<ID3D11Texture2D> back;
    if (!src && swapchain && SUCCEEDED(swapchain->GetBuffer(0, IID_PPV_ARGS(back.GetAddressOf())))) {
        D3D11_TEXTURE2D_DESC d{};
        back->GetDesc(&d);
        src = back.Get();
        box = {0, 0, 0, d.Width, d.Height, 1};
    }
    if (!src) return nullptr;
    D3D11_TEXTURE2D_DESC d{};
    src->GetDesc(&d);
    const std::uint32_t w = box.right - box.left, h = box.bottom - box.top;
    // Only a single-sample source can be copied into the chain.
    if (d.SampleDesc.Count != 1 || !w || !h || !ensure_chain(state.screen_chain, d.Format, w, h) ||
        !copy_into(state.screen_chain, src, box)) {
        return nullptr;
    }
    const float width = state.screen_pose_set ? state.screen_width : kDefaultScreenWidth;
    XrCompositionLayerQuad& quad = layers.quad;
    quad.space         = state.seated.get();
    quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    quad.subImage.swapchain = state.screen_chain.handle.get();
    quad.subImage.imageRect = {{0, 0}, {static_cast<std::int32_t>(w), static_cast<std::int32_t>(h)}};
    quad.pose = state.screen_pose_set ? to_xr(state.screen_pose)
                                      : XrPosef{kIdentityPose.orientation, {0, 0, -kDefaultScreenDistance}};
    quad.size = {width, width * static_cast<float>(h) / static_cast<float>(w)};
    return reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
}

// Second half of a frame: submit what the game rendered for the frame begun
// by the previous frame() and end it, at that frame's own display time:
// copies, Flush, then xrEndFrame (09 §5.3; dumps/12_perf_review.md, P7).
void Session::Impl::end_frame(IDXGISwapChain* swapchain) {
    state.frame_begun = false;
    const XrTime t = state.frame_time;

    FrameLayers layers;
    const XrCompositionLayerBaseHeader* layer = nullptr;
    if (state.should_render && state.views.valid) {
        layer = state.eye_images_set ? projection_layer(layers) : screen_layer(layers, swapchain);
    }

    // Start the GPU on the copies now: the runtime's xrEndFrame waits for
    // them, and the immediate context would otherwise hold them until the
    // next flush.
    if (layer) state.context->Flush();

    XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
    ei.displayTime          = t;
    ei.environmentBlendMode = state.blend;
    ei.layerCount           = layer ? 1 : 0;
    ei.layers               = layer ? &layer : nullptr;
    check_frame(xrEndFrame(state.session.get(), &ei), "xrEndFrame", state.end_result);
}

// Controllers at the start of the frame, so the game's next simulation step
// reads them (P10, 04 §5.9). Their grip poses are located at the same time
// in the same space as the views, and published with them.
void Session::Impl::read_hands(XrTime t) {
    input.sync(t, state.seated.get());
    for (std::size_t h = 0; h < core::vr::kHandCount; ++h) {
        const auto& s = input.state().hands[h];
        state.views.hands[h] = {s.grip, s.grip_velocity, s.grip_valid, s.grip_valid && s.grip_velocity_valid};
    }
}

// The views for this frame, in the seated space, with the invalid-pose
// rules of 09 §5.5. Views without a valid orientation are not valid (no
// projection layer, no FOV, eye offsets kept) and keep the previous values
// rather than whatever the runtime wrote. Views whose positions alone are
// invalid stay valid with each eye at its last valid position, matching the
// held head.
void Session::Impl::locate_views(XrTime t) {
    XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = t;
    li.space       = state.seated.get();
    XrViewState vs{XR_TYPE_VIEW_STATE};
    std::array<XrView, core::vr::kEyeCount> xv{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    std::uint32_t nv = 0;
    const bool located =
        XR_SUCCEEDED(xrLocateViews(state.session.get(), &li, &vs, static_cast<std::uint32_t>(xv.size()), &nv,
                                   xv.data())) &&
        nv == xv.size() && all_finite(from_xr(xv[0].pose)) && all_finite(from_xr(xv[1].pose));
    FrameViews& views = state.views;
    views.valid = located && (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
    views.eye_positions_valid = views.valid && (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT);
    if (!views.valid) return;
    for (std::size_t i = 0; i < core::vr::kEyeCount; ++i) {
        views.eyes[i] = {from_xr(xv[i].pose), from_xr(xv[i].fov)};
        core::vr::hold_position(state.last_eye_pos[i], views.eyes[i].pose.position, views.eye_positions_valid);
    }
}

// The head: without a valid orientation the last head stays whole; with the
// orientation alone, the new orientation at the last valid position
// (09 §5.5).
void Session::Impl::locate_head(XrTime t) {
    Pose head{};
    const XrSpaceLocationFlags hf = locate_flags(state.view.get(), state.seated.get(), t, head);
    core::vr::hold_pose(state.last_head, head, (hf & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0,
                        (hf & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0);
    state.views.head = state.last_head;
    state.views.head_valid = (hf & kOrientationAndPosition) == kOrientationAndPosition;
}

void Session::Impl::log_ipd_once() {
    if (!state.views.eye_positions_valid || state.ipd_logged) return;
    const Vec3 a = state.views.eyes[0].pose.position, b = state.views.eyes[1].pose.position;
    logf(log, kLogIpd, std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z)));
    state.ipd_logged = true;
}

// First half of a frame: wait for the runtime to want it, begin it, and
// locate the views and the controllers at its predicted display time.
bool Session::Impl::begin_frame() {
    XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs{XR_TYPE_FRAME_STATE};
    if (!check_frame(xrWaitFrame(state.session.get(), &wi, &fs), "xrWaitFrame", state.wait_result)) return false;
    XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
    if (!check_frame(xrBeginFrame(state.session.get(), &bi), "xrBeginFrame", state.begin_result)) return false;
    const XrTime t = fs.predictedDisplayTime;
    state.frame_begun   = true;
    state.should_render = fs.shouldRender != XR_FALSE;
    state.frame_time    = t;
    state.frame_period  = fs.predictedDisplayPeriod;

    if (state.recenter_pending && do_recenter(t)) state.recenter_pending = false;

    read_hands(t);
    locate_views(t);
    locate_head(t);
    state.views.display_time = t;
    published_views.store(state.views);
    log_ipd_once();
    return true;
}

bool Session::Impl::frame(IDXGISwapChain* swapchain) noexcept {
    if (!state.session) return false;
    if (state.frame_begun) end_frame(swapchain);
    if (state.lost) return false;
    poll_events();
    if (!state.running) return false;
    check_visible();
    return begin_frame();
}

} // namespace omvr::platform::openxr
