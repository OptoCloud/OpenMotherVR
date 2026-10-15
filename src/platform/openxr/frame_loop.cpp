// The OpenXR frame boundary: end the frame the previous call began, then
// begin the next one.

#include "core/config/settings_store.h" // R: vr (A1): the debug flag through the Log port
#include "core/diag/logger.h"           // R: vr (A1): log through an injected Log&
#include "core/diag/tuning.h"
#include "core/vr/tuning.h"
#include "platform/openxr/session.h"

#include <cmath>

namespace omvr::platform::openxr {

namespace {

namespace config = core::config;
namespace diag = core::diag;
using core::vr::kDefaultScreenDistance;
using core::vr::kDefaultScreenWidth;

// Pose-age probe: logged with Dev. Debug Options every this many frames.
// R: vr (A1): the counters and their rule into core/vr/pose_age.
constexpr std::uint32_t kPoseAgeLogFrames = core::diag::kDebugLogPeriodFrames;

} // namespace

void Session::count_pose_age(XrTime rendered_for) {
    // Unknown: no prediction recorded for the views, or no display period.
    const bool known = g.frame_period > 0 && rendered_for != 0;
    const double periods = known
                               ? static_cast<double>(g.frame_time - rendered_for) / static_cast<double>(g.frame_period)
                               : 0.0;
    const long long whole = std::llround(periods);
    if (known && whole == 0) ++g.age_same;
    else if (known && whole == 1) ++g.age_one;
    else ++g.age_other;
    if (known) {
        g.age_sum += periods;
        ++g.age_known;
    }
    if (++g.age_frames < kPoseAgeLogFrames) return;
    if (config::current().debug.debug) {
        // The mean is over the frames whose age is known (11 §4.1).
        diag::logf("openxr: pose age over {} frames: {} same frame, {} one period early, {} other, "
                   "mean {:.2f} periods over {} known",
                   g.age_frames, g.age_same, g.age_one, g.age_other,
                   g.age_known ? g.age_sum / g.age_known : 0.0, g.age_known);
    }
    g.age_frames = g.age_same = g.age_one = g.age_other = g.age_known = 0;
    g.age_sum = 0.0;
}

// Second half of a frame: submit what the game rendered for the frame begun
// by the previous frame() and end it, at that frame's own display time:
// copies, Flush, then xrEndFrame (dumps/12_perf_review.md, P7).
void Session::end_frame(IDXGISwapChain* swapchain) {
    g.frame_begun = false;
    const XrTime t = g.frame_time;

    // Layers.
    XrCompositionLayerProjection proj{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::array<XrCompositionLayerProjectionView, 2> pv{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
                                                        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
    XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    const XrCompositionLayerBaseHeader* layer = nullptr;

    if (g.should_render && g.views.valid) {
        if (g.eye_images_set) {
            bool ok = true;
            for (int i = 0; i < 2 && ok; ++i) {
                ID3D11Texture2D* tex = g.eye_images.texture[i];
                if (!tex) { ok = false; break; }
                D3D11_TEXTURE2D_DESC d{};
                tex->GetDesc(&d);
                const D3D11_BOX& box = g.eye_images.region[i];
                ok = ensure_chain(g.eye_chain[i], d.Format, box.right - box.left, box.bottom - box.top) &&
                     copy_into(g.eye_chain[i], tex, box);
                pv[i].pose = to_xr(g.eye_images.view[i].pose);
                pv[i].fov  = {g.eye_images.view[i].angle_left, g.eye_images.view[i].angle_right,
                              g.eye_images.view[i].angle_up, g.eye_images.view[i].angle_down};
                pv[i].subImage.swapchain = g.eye_chain[i].handle;
                pv[i].subImage.imageRect = {{0, 0}, {static_cast<std::int32_t>(g.eye_chain[i].width),
                                                     static_cast<std::int32_t>(g.eye_chain[i].height)}};
            }
            if (ok) {
                proj.space     = g.seated;
                proj.viewCount = 2;
                proj.views     = pv.data();
                layer = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proj);
                count_pose_age(g.eye_images.rendered_for);
            }
        } else {
            // Virtual screen: a region of the given image, or else the
            // finished back buffer, on a world-locked quad.
            ID3D11Texture2D* src = g.screen_image;
            D3D11_BOX box = g.screen_region;
            ID3D11Texture2D* back = nullptr;
            if (!src && swapchain &&
                SUCCEEDED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) {
                D3D11_TEXTURE2D_DESC d{};
                back->GetDesc(&d);
                src = back;
                box = {0, 0, 0, d.Width, d.Height, 1};
            }
            if (src) {
                D3D11_TEXTURE2D_DESC d{};
                src->GetDesc(&d);
                const std::uint32_t w = box.right - box.left, h = box.bottom - box.top;
                if (d.SampleDesc.Count == 1 && w && h && ensure_chain(g.screen_chain, d.Format, w, h) &&
                    copy_into(g.screen_chain, src, box)) {
                    const float width = g.screen_pose_set ? g.screen_width : kDefaultScreenWidth;
                    quad.space         = g.seated;
                    quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                    quad.subImage.swapchain = g.screen_chain.handle;
                    quad.subImage.imageRect = {{0, 0}, {static_cast<std::int32_t>(w), static_cast<std::int32_t>(h)}};
                    quad.pose = g.screen_pose_set ? to_xr(g.screen_pose)
                                                  : XrPosef{{0, 0, 0, 1}, {0, 0, -kDefaultScreenDistance}};
                    quad.size = {width, width * static_cast<float>(h) / static_cast<float>(w)};
                    layer = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
                }
            }
            if (back) back->Release();
        }
    }

    // Start the GPU on the copies now: the runtime's xrEndFrame waits for
    // them, and the immediate context would otherwise hold them until the
    // next flush.
    if (layer) g.context->Flush();

    XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
    ei.displayTime          = t;
    ei.environmentBlendMode = g.blend;
    ei.layerCount           = layer ? 1 : 0;
    ei.layers               = layer ? &layer : nullptr;
    check_frame(xrEndFrame(g.session, &ei), "xrEndFrame", g.end_result);
}

// First half of a frame: wait for the runtime to want it, begin it, and
// locate the views and the controllers at its predicted display time.
bool Session::begin_frame() {
    XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs{XR_TYPE_FRAME_STATE};
    if (!check_frame(xrWaitFrame(g.session, &wi, &fs), "xrWaitFrame", g.wait_result)) return false;
    XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
    if (!check_frame(xrBeginFrame(g.session, &bi), "xrBeginFrame", g.begin_result)) return false;
    const XrTime t = fs.predictedDisplayTime;
    g.frame_begun   = true;
    g.should_render = fs.shouldRender != XR_FALSE;
    g.frame_time    = t;
    g.frame_period  = fs.predictedDisplayPeriod;

    if (g.recenter_pending && do_recenter(t)) g.recenter_pending = false;

    // Controllers at the start of the frame, so the game's next simulation
    // step reads them (P10). Their grip poses are located at the same time
    // in the same space as the views, and published with them.
    input.sync(t, g.seated);
    for (int h = 0; h < 2; ++h) {
        const auto& s = input.state().hands[h];
        g.views.hands[h] = {s.grip, s.grip_velocity, s.grip_valid, s.grip_valid && s.grip_velocity_valid};
    }

    // Views for this frame, in the seated space.
    XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = t;
    li.space       = g.seated;
    XrViewState vs{XR_TYPE_VIEW_STATE};
    std::array<XrView, 2> xv{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    std::uint32_t nv = 0;
    const bool located = XR_SUCCEEDED(xrLocateViews(g.session, &li, &vs, 2, &nv, xv.data())) && nv == 2 &&
                         all_finite(from_xr(xv[0].pose)) && all_finite(from_xr(xv[1].pose));
    // Invalid-pose rules (09 §5.5). Views without a valid orientation are
    // not valid (no projection layer, no FOV, eye offsets kept) and keep
    // the previous values rather than whatever the runtime wrote. Views
    // whose positions alone are invalid stay valid with each eye at its
    // last valid position, matching the held head below.
    g.views.valid = located && (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
    g.views.eye_positions_valid = g.views.valid && (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT);
    if (g.views.valid) {
        for (int i = 0; i < 2; ++i) {
            g.views.eyes[i] = {from_xr(xv[i].pose), xv[i].fov.angleLeft, xv[i].fov.angleRight,
                               xv[i].fov.angleUp, xv[i].fov.angleDown};
            if (g.views.eye_positions_valid) g.last_eye_pos[i] = g.views.eyes[i].pose.position;
            else g.views.eyes[i].pose.position = g.last_eye_pos[i];
        }
    }
    // The head: without a valid orientation the last head stays whole; with
    // the orientation alone, the new orientation at the last valid position.
    Pose head{};
    const XrSpaceLocationFlags hf = locate_flags(g.view, g.seated, t, head);
    if (hf & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
        g.last_head.orientation = head.orientation;
        if (hf & XR_SPACE_LOCATION_POSITION_VALID_BIT) g.last_head.position = head.position;
    }
    g.views.head = g.last_head;
    g.views.head_valid = (hf & kOrientationAndPosition) == kOrientationAndPosition;
    g.views.display_time = t;
    g_views.store(g.views);
    if (g.views.eye_positions_valid && !g.ipd_logged) {
        const Vec3 a = g.views.eyes[0].pose.position, b = g.views.eyes[1].pose.position;
        diag::logf("openxr: IPD {:.4f} m", std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) +
                                                     (b.z - a.z) * (b.z - a.z)));
        g.ipd_logged = true;
    }
    return true;
}

bool Session::frame(IDXGISwapChain* swapchain) noexcept {
    if (g.session == XR_NULL_HANDLE) return false;
    if (g.frame_begun) end_frame(swapchain);
    if (g.lost) return false;
    poll_events();
    if (!g.running) return false;
    check_visible();
    return begin_frame();
}

} // namespace omvr::platform::openxr
