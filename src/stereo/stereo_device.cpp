// The stereo manager and device the game is given in place of its own.
//
// From: FUN_10003510 / FUN_10003640 (init), FUN_1001eac0 (device init),
// FUN_1001f6a0 (CreateDevice), FUN_100202a0 (GetRenderTargetSize hook), the
// StereoInterface vtable 0x1005a5c0 (FUN_1001ed50 .. FUN_1001f300) and the
// StereoManager vtable 0x1005a61c (FUN_10020230, FUN_10020290).
// Analysis: dumps/06a_stereo_interface.md.

#include "stereo_device.h"

#include "game_hooks.h"

#include "config/config.h"
#include "diag/log.h"
#include "hook/ai_addresses.h"
#include "integration/script_bridge.h"
#include "runtime/vr_detect.h"

#include <d3d11.h>
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string_view>
#include <utility>

namespace mothervr::stereo {

namespace {

#include "mirror_shaders.inc"

using hook::AiAddr;
using hook::ai_address;

StereoManager g_manager{};

// The backend behind the device. A plain pointer, not a unique_ptr, so no
// static destructor calls into the VR runtime at process exit: that runs in
// DLL_PROCESS_DETACH under the loader lock, where VR_Shutdown or
// xrDestroyInstance (which wait on runtime threads) can deadlock. A session
// still open at exit is left to the OS and the runtime, which both handle a
// process that ends. Main thread only.
runtime::HmdBackend* g_backend = nullptr;
bool g_runtime_failed = false;

void* g_device_vtable[22]{};
void* g_manager_vtable[3]{};

// ---- Game renderer accessors (plain __cdecl in AI.exe) -----------------------

void* renderer() noexcept {
    return *reinterpret_cast<void**>(ai_address(AiAddr::RendererPtr));
}

template <class T>
T* renderer_get(AiAddr slot) noexcept {
    using Fn = T* (__cdecl*)(void*);
    return reinterpret_cast<Fn>(ai_address(slot))(renderer());
}

template <class T>
T* renderer_rt(AiAddr slot, std::uint16_t index) noexcept {
    using Fn = T* (__cdecl*)(void*, std::uint32_t);
    return reinterpret_cast<Fn>(ai_address(slot))(renderer(), index);
}

template <class T>
void release(T*& p) noexcept {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

StereoInterface& device_of(void* self) noexcept { return *static_cast<StereoInterface*>(self); }

// Both eyes side by side: left half, right half.
void set_target_size(StereoInterface& si, int width, int height) noexcept {
    si.rt_width    = width;
    si.rt_height   = height;
    si.viewport[0] = {0, 0, width / 2, height};
    si.viewport[1] = {width / 2, 0, width / 2, height};
}

// ---- Device init (non-virtual) -------------------------------------------------

// Deletes the backend, whose destructor shuts its runtime down.
void drop_backend(StereoInterface& si) noexcept {
    si.backend = nullptr;
    delete std::exchange(g_backend, nullptr);
}

// The runtime cannot drive the headset after all: detach the device, as a
// failed init leaves it, and tell the player.
void fail_runtime(StereoInterface& si, runtime::detect_result::Chosen chosen) noexcept {
    store_state(si, DeviceState::None);
    if (load_device(g_manager) == &si) store_device(g_manager, nullptr);
    integration::g_vr_session_active = false;
    drop_backend(si);
    g_runtime_failed = true;
    runtime::report_init_failure(chosen);
}

// -vrscale <s>: world scale, 1.0 outside [0.495, 1.505].
// From: FUN_1001f700 (the -vrforce* switches are handled by vr_detect).
float world_scale_from_command_line() noexcept {
    constexpr std::wstring_view kSwitch = L"-vrscale";
    const wchar_t* at = std::wcsstr(::GetCommandLineW(), kSwitch.data());
    if (!at) return 1.0f;
    const float s = static_cast<float>(std::wcstod(at + kSwitch.size(), nullptr));
    return (s >= 0.495f && s <= 1.505f) ? s : 1.0f;
}

// From: FUN_1001eac0.
void init_device(StereoInterface& si) noexcept {
    store_state(si, DeviceState::None);
    si.frame_index = 0;
    si.hmd = nullptr;
    si.rt_width = si.rt_height = 0;

    const auto chosen = runtime::last_result().chosen;
    if (chosen == runtime::detect_result::Chosen::OpenVR) {
        g_backend = runtime::make_openvr_backend().release();
        si.runtime_id = static_cast<std::int32_t>(config::VRRuntime::OpenVR);
    } else if (chosen == runtime::detect_result::Chosen::OpenXR) {
        g_backend = runtime::make_openxr_backend().release();
        si.runtime_id = static_cast<std::int32_t>(config::VRRuntime::OpenXR);
    } else {
        diag::logf("[stereo] no stereo device for runtime {}", runtime::to_string(chosen));
        return;
    }
    si.backend = g_backend;
    if (!si.backend->init()) {
        diag::log_line("[stereo] HMD backend init failed, running without VR");
        fail_runtime(si, chosen);
        return;
    }
    si.backend->register_control_hints();

    // A plausible ovrHmdDesc header for the game: a DK2.
    si.desc.handle          = reinterpret_cast<void*>(static_cast<std::uintptr_t>(si.handle_seed));
    si.desc.type            = 6;      // ovrHmd_DK2
    si.desc.hmd_caps        = 0x83;
    si.desc.sensor_caps     = 0x70;
    si.desc.distortion_caps = 2;
    si.backend->get_hmd_desc(si.desc);
    si.eye_fov[0] = si.desc.max_eye_fov[0];
    si.eye_fov[1] = si.desc.max_eye_fov[1];
    store_state(si, DeviceState::Ready);
}

// ---- Mirror (desktop window) ----------------------------------------------------

// The 48-byte constant buffer of the mirror shaders.
struct MirrorConstants {
    float draw_top_left[2];
    float draw_bottom_right[2];
    float sample_top_left[2];
    float sample_bottom_right[2];
    float scale_adjust;
    float window_aspect;
    float pad[2];
};
static_assert(sizeof(MirrorConstants) == 0x30);

// Which part of the eye texture to show in the window, letterboxed: both
// eyes, the left half or the right half. Without "symmetric", each eye is
// cropped from the symmetric render FOV back to its real FOV.
// From: FUN_1001eda0, the tail (which tests the type as 0 both, 2 left,
// otherwise right: the original's MirrorViewType integers).
MirrorConstants mirror_constants(const StereoInterface& si) noexcept {
    using config::MirrorViewType;
    const auto& cfg = config::current().rendering;
    const MirrorViewType m = cfg.mirror_view_type;
    float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f, xr = 1.0f;
    if (cfg.symmetric_mirror_view) {
        if (m == MirrorViewType::LeftEye)       { u0 = 0.0f; u1 = 0.5f; }
        else if (m == MirrorViewType::RightEye) { u0 = 0.5f; u1 = 1.0f; }
    } else {
        const int e = m == MirrorViewType::RightEye ? 1 : 0;
        const auto& d = si.desc.default_eye_fov[e];
        const auto& x = si.desc.max_eye_fov[e];
        u0 = (0.5f - (d.left / x.left) * 0.5f) * 0.5f;
        u1 = ((d.right / x.right) * 0.5f + 0.5f) * 0.5f;
        v0 = 0.5f - (d.up / x.up) * 0.5f;
        v1 = (d.down / x.down) * 0.5f + 0.5f;
        if (m == MirrorViewType::BothEyes) xr = 0.5f;
    }
    const float rt_aspect  = si.rt_height ? float(si.rt_width) / float(si.rt_height) : 1.0f;
    const float win_aspect = si.mirror_height ? float(si.mirror_width) / float(si.mirror_height) : rt_aspect;
    const float k = win_aspect * xr / ((u1 - u0) * rt_aspect);

    // The original uses 1.0 the first time, then 0.5 / pixel scale: it
    // compensated for eyes that started at half size. Ours are full size
    // from the start, so the mirror fills the window at 1.0.
    constexpr float scale_adjust = 1.0f;

    return {{0.0f, (1.0f - k) * 0.5f}, {xr, (1.0f + k) * 0.5f}, {u0, v0}, {u1, v1},
            scale_adjust, win_aspect, {}};
}

// ---- Device vtable (all __thiscall; see stereo_device.h) -------------------------

// [0] scalar deleting destructor. The object is static: never freed.
void __fastcall dev_release(void* self, void*, unsigned /*flags*/) noexcept {
    auto& si = device_of(self);
    if (si.state == DeviceState::Running) store_state(si, DeviceState::Ready);
    if (si.hmd) store_state(si, DeviceState::None);
}

// [1] StartSensor: the game switches to stereo rendering.
void __fastcall dev_start_sensor(void* self, void*) noexcept {
    auto& si = device_of(self);
    if (si.state == DeviceState::Ready) {
        store_state(si, DeviceState::Running);
        diag::log_line("[stereo] sensor started, stereo rendering on");
    }
}

// [2] StopSensor.
void __fastcall dev_stop_sensor(void* self, void*) noexcept {
    auto& si = device_of(self);
    if (si.state == DeviceState::Running) store_state(si, DeviceState::Ready);
}

// [3] ConfigureRendering(stereo render target): describe the eye texture,
// create the mirror resources. If the backend cannot drive the headset with
// the game's device (no OpenXR session), the device detaches here; the
// mirror resources are still made for any call already on its way.
bool __fastcall dev_configure_rendering(void* self, void*, void* stereo_rt) noexcept {
    auto& si = device_of(self);
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
    if (si.backend && !si.backend->create_render_resources(device, ctx)) {
        diag::log_line("[stereo] HMD backend cannot render, running without VR");
        fail_runtime(si, runtime::last_result().chosen);
    }

    release(si.mirror_vs);
    device->CreateVertexShader(kMirrorVS, sizeof(kMirrorVS), nullptr, &si.mirror_vs);
    release(si.mirror_ps_eyes);
    device->CreatePixelShader(kMirrorPSEyes, sizeof(kMirrorPSEyes), nullptr, &si.mirror_ps_eyes);
    release(si.mirror_ps_screen);
    device->CreatePixelShader(kMirrorPSScreen, sizeof(kMirrorPSScreen), nullptr, &si.mirror_ps_screen);

    release(si.mirror_cb);
    const D3D11_BUFFER_DESC cb_desc{sizeof(MirrorConstants), D3D11_USAGE_DEFAULT,
                                    D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
    device->CreateBuffer(&cb_desc, nullptr, &si.mirror_cb);
    if (si.mirror_cb) {
        const MirrorConstants c = mirror_constants(si);
        ctx->UpdateSubresource(si.mirror_cb, 0, nullptr, &c, 4, 4);
    }

    release(si.mirror_sampler);
    D3D11_SAMPLER_DESC s{};
    s.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    s.MaxAnisotropy  = 1;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER;
    s.MinLOD         = -D3D11_FLOAT32_MAX;
    s.MaxLOD         = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&s, &si.mirror_sampler);

    diag::logf("[stereo] rendering configured: {}x{} texture={}", si.rt_width, si.rt_height,
               static_cast<void*>(t0.texture));
    return true;
}

// [4] ShutdownRendering.
bool __fastcall dev_shutdown_rendering(void* self, void*) noexcept {
    auto& si = device_of(self);
    if (si.backend) si.backend->release_render_resources();
    return true;
}

// [5] BeginFrame: poll the controllers, take the eye offsets.
void __fastcall dev_begin_frame(void* self, void*) noexcept {
    auto& si = device_of(self);
    if (!si.backend) return;
    si.backend->poll_input();
    if (si.backend->refresh_hmd_desc(si.desc)) {
        si.eye_fov[0] = si.desc.max_eye_fov[0];
        si.eye_fov[1] = si.desc.max_eye_fov[1];
    }
    float eye[6]{};
    si.backend->begin_frame(eye, si.pose_fetched != 0);
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

// [6] EndFrame: submit the eyes, draw the mirror, present. LibOVR's
// EndFrame presented in SDK-distortion mode, so the game does not.
void __fastcall dev_end_frame(void* self, void*) noexcept {
    auto& si = device_of(self);
    if (si.backend) si.backend->submit_frame(si.eye_texture[0].texture);

    auto* swap = renderer_get<IDXGISwapChain>(AiAddr::RendererSwapChain);
    auto* ctx  = renderer_get<ID3D11DeviceContext>(AiAddr::RendererContext);

    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (rtv) {
        const float dark_red[4] = {0.6f, 0.0f, 0.0f, 1.0f};
        ctx->ClearRenderTargetView(rtv, dark_red);
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        // Draw over the whole window, not whatever viewport the game left.
        ID3D11Resource* res = nullptr;
        rtv->GetResource(&res);
        if (ID3D11Texture2D* tex = nullptr;
            res && SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex)))) {
            D3D11_TEXTURE2D_DESC td{};
            tex->GetDesc(&td);
            const D3D11_VIEWPORT vp{0.0f, 0.0f, float(td.Width), float(td.Height), 0.0f, 1.0f};
            ctx->RSSetViewports(1, &vp);
            tex->Release();
        }
        if (res) res->Release();
    }
    ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    ctx->RSSetState(nullptr);
    ctx->VSSetShader(si.mirror_vs, nullptr, 0);
    // The eyes while the game tracks the head, else what is bound at t0
    // (menus, videos).
    ctx->PSSetShader(si.pose_fetched ? si.mirror_ps_eyes : si.mirror_ps_screen, nullptr, 0);
    ctx->PSSetShaderResources(1, 1, &si.eye_texture[0].srv);
    ctx->PSSetConstantBuffers(0, 1, &si.mirror_cb);
    ctx->PSSetSamplers(0, 1, &si.mirror_sampler);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);
    if (rtv) rtv->Release();  // the original leaks this reference every frame

    swap->Present(0, 0);
    si.pose_fetched = 0;
}

// [7] BeginEyeRender / [8] EndEyeRender: nothing to do.
void __fastcall dev_eye_render_nop(void*, void*, int /*eye*/) noexcept {}

// [9] IncrementFrameIndex.
void __fastcall dev_increment_frame(void* self, void*) noexcept { ++device_of(self).frame_index; }

// [10] IsReady (name uncertain).
bool __fastcall dev_is_ready(void*, void*) noexcept { return true; }

// [11] GetHeadPose(float out[12]).
float* __fastcall dev_head_pose(void* self, void*, float* out) noexcept {
    auto& si = device_of(self);
    if (si.backend) si.backend->head_pose(*reinterpret_cast<runtime::Matrix34*>(out), si.world_scale);
    si.pose_fetched = 1;
    return out;
}

// [15] GetEyeViewportRect(eye, half_width, height, &left, &top, &right,
// &bottom) for a target of the given size. The game's version ignores the
// size and returns the main target's eye halves, which is wrong for the
// half-size reflection target once that is rendered per eye.
void __fastcall dev_eye_viewport(void*, void*, int eye, int half_width, int height,
                                 int* left, int* top, int* right, int* bottom) noexcept {
    const int x = eye == 1 ? half_width : 0;
    *left   = x;
    *top    = 0;
    *right  = x + half_width;
    *bottom = height;
}

// [16] GetRenderTargetSize(int* w, int* h): both eyes side by side, and
// the eye viewports.
//
// The original asks for half scale until stereo runs. The game sizes its
// targets from that first answer (the same call picks the display mode) and
// does not rebuild them when stereo starts, so the eyes stay at half
// resolution. We use the configured pixel scale from the start; the
// swapchain is kept windowed so the mode change cannot fail (d3d11_proxy).
int* __fastcall dev_render_target_size(void* self, void*, int* w, int* h) noexcept {
    auto& si = device_of(self);
    if (!si.backend) return w;
    // PixelScale multiplies the runtime's recommended size, which already
    // includes SteamVR's own resolution setting. Capped so the side-by-side
    // target stays within D3D11's 16384 texture limit.
    constexpr float kMaxTextureSize = 16384.0f;
    const runtime::Sizei l1 = si.backend->eye_texture_size(0, 1.0f);
    const runtime::Sizei r1 = si.backend->eye_texture_size(1, 1.0f);
    float scale = config::current().rendering.pixel_scale;
    if (l1.w + r1.w > 0 && l1.h > 0 && r1.h > 0) {
        const float max_scale = std::min(kMaxTextureSize / static_cast<float>(l1.w + r1.w),
                                         kMaxTextureSize / static_cast<float>(std::max(l1.h, r1.h)));
        scale = std::min(scale, max_scale);
    }
    const runtime::Sizei l = si.backend->eye_texture_size(0, scale);
    const runtime::Sizei r = si.backend->eye_texture_size(1, scale);
    const int width  = l.w + r.w;
    const int height = l.w <= r.w ? r.h : l.h;   // compares widths, as the original

    // Keep the eye layout of the texture in use until the game makes one of
    // the new size (it then calls ConfigureRendering again).
    const bool configured = si.configured_width != 0;
    if (!configured || (width == si.configured_width && height == si.configured_height)) {
        set_target_size(si, width, height);
    }
    static int s_last_w = 0, s_last_h = 0;
    if (width != s_last_w || height != s_last_h) {
        s_last_w = width;
        s_last_h = height;
        diag::logf("[stereo] render target size {}x{} ({:.1f} MP, pixel scale {:.2f}; state {}, texture {}x{})",
                   width, height, static_cast<double>(width) * height / 1e6, scale,
                   static_cast<int>(si.state), si.configured_width, si.configured_height);
    }
    *w = width;
    *h = height;
    return w;
}

// [17] GetHmdResolution(int*, int*): the window keeps its size.
int* __fastcall dev_hmd_resolution(void*, void*, int* /*w*/, int* h) noexcept { return h; }

// [18] GetDisplayDeviceName(char* buf, int size): no display to name.
char* __fastcall dev_display_name(void*, void*, char* buf, int /*size*/) noexcept { return buf; }

// [19] IsPositionTracked. The game's version takes no arguments; the
// original's shared stub popped 8 bytes. No caller found either way.
bool __fastcall dev_position_tracked(void*, void*) noexcept { return true; }

// [20] Unused.
void __fastcall dev_unused(void*, void*) noexcept {}

// [21] RecenterAndGetHeadPose(float out[12]): MotherVR's extra slot. The
// game's recenter call sites are redirected here from slot 11.
float* __fastcall dev_recenter_pose(void* self, void*, float* out) noexcept {
    auto& si = device_of(self);
    if (si.backend) si.backend->head_yaw_pose(*reinterpret_cast<runtime::Matrix34*>(out), si.world_scale);
    return out;
}

// ---- Manager vtable ---------------------------------------------------------------

// [0] scalar deleting destructor: static object.
void __fastcall mgr_release(void*, void*, unsigned) noexcept {}

// [1] OnEvent(Event*). Event 0x18 starts the sensor and switches four
// graphics options off/on.
void __fastcall mgr_on_event(StereoManager* self, void*, const std::int32_t* event) noexcept {
    if (*event != 0x18 || !self->device) return;
    reinterpret_cast<void (__fastcall*)(void*, void*)>(self->device->vtable[1])(self->device, nullptr);

    using SetOptionFn = void (__fastcall*)(void* settings, void* edx, int option, const char* value);
    auto set_option = reinterpret_cast<SetOptionFn>(ai_address(AiAddr::GraphicsSetOption));
    void* settings  = ai_address(AiAddr::GraphicsSettings);
    set_option(settings, nullptr, 0x00, "Off");
    set_option(settings, nullptr, 0x15, "Off");
    set_option(settings, nullptr, 0x0D, "Off");
    set_option(settings, nullptr, 0x07, "On");
}

// [2] OnEvent2(Event*, int): always handled.
bool __fastcall mgr_on_event2(void*, void*, const void*, int) noexcept { return true; }

template <class Fn>
void* as_slot(Fn fn) noexcept { return reinterpret_cast<void*>(fn); }

} // namespace

StereoManager& manager() noexcept { return g_manager; }

bool runtime_failed() noexcept { return g_runtime_failed; }

void initialize() noexcept {
    g_device_vtable[0]  = as_slot(&dev_release);
    g_device_vtable[1]  = as_slot(&dev_start_sensor);
    g_device_vtable[2]  = as_slot(&dev_stop_sensor);
    g_device_vtable[3]  = as_slot(&dev_configure_rendering);
    g_device_vtable[4]  = as_slot(&dev_shutdown_rendering);
    g_device_vtable[5]  = as_slot(&dev_begin_frame);
    g_device_vtable[6]  = as_slot(&dev_end_frame);
    g_device_vtable[7]  = as_slot(&dev_eye_render_nop);
    g_device_vtable[8]  = as_slot(&dev_eye_render_nop);
    g_device_vtable[9]  = as_slot(&dev_increment_frame);
    g_device_vtable[10] = as_slot(&dev_is_ready);
    g_device_vtable[11] = as_slot(&dev_head_pose);
    // The game's own device code for eye transforms and FOV. It reads our
    // device through the game layout.
    g_device_vtable[12] = ai_address(AiAddr::GameDeviceEyeTransform);
    g_device_vtable[13] = ai_address(AiAddr::GameDeviceFovAspect);
    g_device_vtable[14] = ai_address(AiAddr::GameDeviceEyeScaleOffset);
    g_device_vtable[15] = as_slot(&dev_eye_viewport);
    g_device_vtable[16] = as_slot(&dev_render_target_size);
    g_device_vtable[17] = as_slot(&dev_hmd_resolution);
    g_device_vtable[18] = as_slot(&dev_display_name);
    g_device_vtable[19] = as_slot(&dev_position_tracked);
    g_device_vtable[20] = as_slot(&dev_unused);
    g_device_vtable[21] = as_slot(&dev_recenter_pose);

    g_manager_vtable[0] = as_slot(&mgr_release);
    g_manager_vtable[1] = as_slot(&mgr_on_event);
    g_manager_vtable[2] = as_slot(&mgr_on_event2);

    // Manager: game defaults (FUN_10003510).
    auto& m = g_manager;
    m.vtable = g_manager_vtable;
    for (int i = 0; i < 4; ++i) m.game_matrix[i * 5] = 1.0f;
    m.game_64 = 1.0f;
    m.game_a8 = 1;
    m.fade    = 1.0f;
    // Reference pose rows (-1,0,0,0), (0,-1,0,0), (0,0,1,0); the game uses identity.
    m.reference_pose[0]  = -1.0f;
    m.reference_pose[5]  = -1.0f;
    m.reference_pose[10] =  1.0f;

    // Device (FUN_10003640).
    auto& si = m.si;
    si.vtable      = g_device_vtable;
    si.world_scale = 1.0f;
}

void __fastcall create_device(StereoManager* self, void*, int enable) noexcept {
    if (enable != 1) return;
    auto& si = self->si;
    // The game can call this again (a display reset). A second VR_Init or
    // overlay would fail, and a runtime that failed has told the player
    // already: start the runtime once, then re-attach what it gave.
    static bool s_created = false;
    const bool first = !std::exchange(s_created, true);
    if (first) {
        si.world_scale = world_scale_from_command_line();
        init_device(si);
    }
    store_device(*self, si.state != DeviceState::None ? &si : nullptr);
    integration::g_vr_session_active = self->device != nullptr;
    diag::logf("[stereo] CreateDevice{}: {}", first ? "" : " again", self->device ? "HMD attached" : "no HMD");
    if (first && self->device && si.state == DeviceState::Ready) {
        dev_recenter_pose(&si, nullptr, self->reference_pose);
        // The headset may be lying on the desk at launch: take the
        // reference again once it is worn.
        if (!si.backend->headset_in_use()) request_recenter();
    }
}

int* __fastcall get_render_target_size(StereoManager* self, void*, int* w, int* h) noexcept {
    StereoInterface* si = self->device;
    if (!si) return h;
    si->mirror_width  = *w;
    si->mirror_height = *h;
    return reinterpret_cast<int* (__fastcall*)(void*, void*, int*, int*)>(si->vtable[16])(si, nullptr, w, h);
}

int* __fastcall get_hmd_resolution(StereoManager*, void*, int* /*w*/, int* h) noexcept { return h; }

} // namespace mothervr::stereo
