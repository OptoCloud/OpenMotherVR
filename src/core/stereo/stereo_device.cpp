// The stereo manager and device the game is given in place of its own.
//
// From: FUN_10003510 / FUN_10003640 (init), FUN_1001eac0 (device init),
// FUN_1001f6a0 (CreateDevice), FUN_100202a0 (GetRenderTargetSize hook), the
// StereoInterface vtable 0x1005a5c0 (FUN_1001ed50 .. FUN_1001f300) and the
// StereoManager vtable 0x1005a61c (FUN_10020230, FUN_10020290).
// Analysis: dumps/06a_stereo_interface.md.

#include "core/stereo/stereo_device.h"

#include "core/config/launch_options.h"
#include "core/config/settings.h"
#include "core/diag/logger.h"
#include "core/stereo/mirror_layout.h"
#include "core/stereo/render_target.h"
#include "core/ui/compat_messages.h"
#include "core/vr/runtime_detector.h"
#include "game/stereo/renderer.h"                  // R: stereo (A3): the thunks hand in the render target
#include "game/stereo/stereo_objects.h"            // R: stereo (A3): the thunks write the game layout
#include "platform/d3d11/handles.h"                // R: stereo (A3): the thunks hand in GPU handles
#include "platform/d3d11/render/mirror_renderer.h" // R: stereo (A3): through the DesktopMirror port

#include <d3d11.h> // R: stereo (A3): the texture size handed in by the thunk

#include <atomic>
#include <cstdint>
#include <utility>

namespace omvr::core::stereo {

namespace {

using game::AiAddr;
using game::DeviceState;
using game::StereoInterface;
using game::StereoManager;
using game::load_backend;
using game::load_pose_fetched;
using game::store_backend;
using game::stereo::load_device;
using game::stereo::load_state;
using game::stereo::renderer_get;
using game::stereo::renderer_rt;
using game::stereo::store_device;
using game::stereo::store_state;

std::atomic<StereoDevice*> g_process_stereo{nullptr};

// The game's LibOVR 0.4 descriptor and the runtime-neutral one: what the
// runtime fills goes back into the game's.
vr::HeadsetDesc headset_desc_of(const game::HmdDesc& d) noexcept {
    vr::HeadsetDesc h;
    h.product_name = d.product_name;
    h.manufacturer = d.manufacturer;
    for (int e = 0; e < 2; ++e) {
        h.default_eye_fov[e] = {d.default_eye_fov[e].up, d.default_eye_fov[e].down, d.default_eye_fov[e].left,
                                d.default_eye_fov[e].right};
        h.max_eye_fov[e] = {d.max_eye_fov[e].up, d.max_eye_fov[e].down, d.max_eye_fov[e].left,
                            d.max_eye_fov[e].right};
    }
    return h;
}

void apply_headset_desc(game::HmdDesc& d, const vr::HeadsetDesc& h) noexcept {
    d.product_name = h.product_name;
    d.manufacturer = h.manufacturer;
    for (int e = 0; e < 2; ++e) {
        d.default_eye_fov[e] = {h.default_eye_fov[e].up, h.default_eye_fov[e].down, h.default_eye_fov[e].left,
                                h.default_eye_fov[e].right};
        d.max_eye_fov[e] = {h.max_eye_fov[e].up, h.max_eye_fov[e].down, h.max_eye_fov[e].left,
                            h.max_eye_fov[e].right};
    }
}

platform::d3d11::MirrorObjects mirror_objects(StereoInterface& si) noexcept {
    return {si.mirror_vs, si.mirror_ps, si.mirror_cb_screen, si.mirror_cb, si.mirror_sampler};
}

} // namespace

bool StereoDevice::running() const noexcept {
    StereoInterface* dev = load_device(g_manager);
    return dev && load_state(*dev) == DeviceState::Running;
}

bool StereoDevice::device_attached() const noexcept { return load_device(g_manager) != nullptr; }

// The runtime cannot drive the headset after all: detach the device, as a
// failed init leaves it, and tell the player. State and pointer are cleared
// before the backend, so no new call starts using it.
//
// At init (game thread, before the device was ever attached) nothing else
// can hold the backend: it is deleted, and its destructor shuts the runtime
// down. A late failure (slot 3, on the render thread) can come while the
// game thread is inside a call into the backend (head pose, recenter):
// the backend is then only taken off the device and kept, unused, until
// the process ends, so that call finishes on a live runtime.
void StereoDevice::fail_runtime(StereoInterface& si, vr::detect_result::Chosen chosen, bool late) noexcept {
    store_state(si, DeviceState::None);
    if (load_device(g_manager) == &si) store_device(g_manager, nullptr);
    g_vr_session_active = false;
    store_backend(si, nullptr);
    if (!late) delete std::exchange(g_backend, nullptr);
    g_runtime_failed = true;
    ui::report_init_failure(dialogs_, chosen);
}

// From: FUN_1001eac0.
void StereoDevice::init_device(StereoInterface& si) noexcept {
    store_state(si, DeviceState::None);
    si.frame_index = 0;
    si.hmd = nullptr;
    si.rt_width = si.rt_height = 0;

    const auto chosen = vr::last_result().chosen;
    if (chosen == vr::detect_result::Chosen::OpenVR) {
        g_backend = make_runtime_(chosen).release();
        si.runtime_id = static_cast<std::int32_t>(config::VRRuntime::OpenVR);
    } else if (chosen == vr::detect_result::Chosen::OpenXR) {
        g_backend = make_runtime_(chosen).release();
        si.runtime_id = static_cast<std::int32_t>(config::VRRuntime::OpenXR);
    } else {
        diag::logf("[stereo] no stereo device for runtime {}", vr::to_string(chosen));
        return;
    }
    store_backend(si, g_backend);
    if (!g_backend->init()) {
        diag::log_line("[stereo] HMD backend init failed, running without VR");
        fail_runtime(si, chosen, false);
        return;
    }
    g_backend->register_control_hints();

    // A plausible ovrHmdDesc header for the game: a DK2.
    si.desc.handle          = reinterpret_cast<void*>(static_cast<std::uintptr_t>(si.handle_seed));
    si.desc.type            = 6;      // ovrHmd_DK2
    si.desc.hmd_caps        = 0x83;
    si.desc.sensor_caps     = 0x70;
    si.desc.distortion_caps = 2;
    {
        vr::HeadsetDesc desc = headset_desc_of(si.desc);
        g_backend->headset_desc(desc);
        apply_headset_desc(si.desc, desc);
    }
    si.eye_fov[0] = si.desc.max_eye_fov[0];
    si.eye_fov[1] = si.desc.max_eye_fov[1];
    store_state(si, DeviceState::Ready);
}

bool StereoDevice::configure_rendering(StereoInterface& si, void* stereo_rt) {
    auto& t0 = si.eye_texture[0];
    t0.api      = 5;
    t0.size     = {si.rt_width, si.rt_height};
    t0.viewport = si.viewport[0];
    if (stereo_rt) {
        const auto index = *reinterpret_cast<std::uint16_t*>(static_cast<std::uint8_t*>(stereo_rt) + 0x80);
        t0.texture = renderer_rt<ID3D11Texture2D>(AiAddr::RendererRtTexture, index);
        t0.srv     = renderer_rt<ID3D11ShaderResourceView>(AiAddr::RendererRtSrv, index);
    }
    // The eye halves must match the texture the game actually created. The
    // original trusts its last size computation, which runs at half scale
    // before stereo starts and at full scale after, so the two can disagree.
    if (t0.texture) {
        D3D11_TEXTURE2D_DESC td{};
        t0.texture->GetDesc(&td);
        set_target_size(si, static_cast<int>(td.Width), static_cast<int>(td.Height));
        si.configured_width  = si.rt_width;
        si.configured_height = si.rt_height;
        t0.size     = {si.rt_width, si.rt_height};
        t0.viewport = si.viewport[0];
    }
    si.eye_texture[1] = t0;
    si.eye_texture[1].viewport = si.viewport[1];

    auto* device = renderer_get<ID3D11Device>(AiAddr::RendererDevice);
    auto* ctx    = renderer_get<ID3D11DeviceContext>(AiAddr::RendererContext);
    if (auto* backend = load_backend(si); backend && !backend->create_render_resources(platform::d3d11::gpu_device(device), platform::d3d11::gpu_context(ctx))) {
        diag::log_line("[stereo] HMD backend cannot render, running without VR");
        fail_runtime(si, vr::last_result().chosen, true);
    }

    platform::d3d11::create_mirror(mirror_objects(si), device, mirror_constants(si, false), mirror_constants(si, true));

    diag::logf("[stereo] rendering configured: {}x{} texture={}", si.rt_width, si.rt_height,
               static_cast<void*>(t0.texture));
    return true;
}

void StereoDevice::begin_frame(StereoInterface& si) {
    auto* backend = load_backend(si);
    if (!backend) return;
    backend->poll_input();
    if (vr::HeadsetDesc desc = headset_desc_of(si.desc); backend->refresh_headset_desc(desc)) {
        apply_headset_desc(si.desc, desc);
        si.eye_fov[0] = si.desc.max_eye_fov[0];
        si.eye_fov[1] = si.desc.max_eye_fov[1];
    }
    float eye[6]{};
    backend->begin_frame(eye, load_pose_fetched(si));
    // The left eye's offset goes to the game's eye 1, as in the original.
    const float s = si.world_scale;
    for (int i = 0; i < 3; ++i) {
        si.eye_desc[1].view_adjust[i] = s * eye[i];
        si.eye_desc[0].view_adjust[i] = s * eye[3 + i];
    }
    static bool s_logged = false;
    if (!std::exchange(s_logged, true)) {
        diag::logf("[stereo] eye offsets: eye0 ({:.4f} {:.4f} {:.4f}) eye1 ({:.4f} {:.4f} {:.4f})",
                   si.eye_desc[0].view_adjust[0], si.eye_desc[0].view_adjust[1], si.eye_desc[0].view_adjust[2],
                   si.eye_desc[1].view_adjust[0], si.eye_desc[1].view_adjust[1], si.eye_desc[1].view_adjust[2]);
    }
}

void StereoDevice::create_device(StereoManager* self) {
    auto& si = self->si;
    // The game can call this again (a display reset). A second VR_Init or
    // overlay would fail, and a runtime that failed has told the player
    // already: start the runtime once, then re-attach what it gave.
    static bool s_created = false;
    const bool first = !std::exchange(s_created, true);
    if (first) {
        si.world_scale = config::world_scale_from_command_line();
        init_device(si);
    }
    StereoInterface* device = load_state(si) != DeviceState::None ? &si : nullptr;
    store_device(*self, device);
    g_vr_session_active = device != nullptr;
    diag::logf("[stereo] CreateDevice{}: {}", first ? "" : " again", device ? "HMD attached" : "no HMD");
    if (first && device && load_state(si) == DeviceState::Ready) {
        reinterpret_cast<float* (__fastcall*)(void*, void*, float*)>(si.vtable[21])(&si, nullptr, self->reference_pose);
        // The headset may be lying on the desk at launch: take the
        // reference again once it is worn (a pose that was not valid has
        // queued the retake already).
        if (auto* backend = load_backend(si); backend && !backend->headset_in_use()) update_.request_recenter();
    }
}

void set_process_stereo(StereoDevice* stereo) noexcept { g_process_stereo.store(stereo, std::memory_order_release); }

bool runtime_failed() noexcept { return g_process_stereo.load(std::memory_order_acquire)->runtime_failed(); }

bool vr_session_active() noexcept { return g_process_stereo.load(std::memory_order_acquire)->vr_session_active(); }

} // namespace omvr::core::stereo
