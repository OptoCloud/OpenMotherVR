// The stereo manager and device the game is given in place of its own.
//
// From: FUN_10003510 / FUN_10003640 (init), FUN_1001eac0 (device init),
// FUN_1001f6a0 (CreateDevice), FUN_100202a0 (GetRenderTargetSize hook), the
// StereoInterface vtable 0x1005a5c0 (FUN_1001ed50 .. FUN_1001f300) and the
// StereoManager vtable 0x1005a61c (FUN_10020230, FUN_10020290).
// Analysis: dumps/06a_stereo_interface.md.

#include "stereo_device.h"

#include "game_hooks.h"
#include "guard.h"

#include "config/config.h"
#include "diag/log.h"
#include "hook/ai_addresses.h"
#include "integration/script_bridge.h"
#include "runtime/vr_detect.h"

#include <d3d11.h>
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstring>
#include <cwchar>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
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
// process that ends. Owns the backend; after a late detach (fail_runtime)
// it keeps it, unused, until exit.
runtime::HmdBackend* g_backend = nullptr;
// Set on the game thread (init) or the render thread (late detach); read
// by the Present hook and the options page.
std::atomic<bool> g_runtime_failed{false};

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
void fail_runtime(StereoInterface& si, runtime::detect_result::Chosen chosen, bool late) noexcept {
    store_state(si, DeviceState::None);
    if (load_device(g_manager) == &si) store_device(g_manager, nullptr);
    integration::g_vr_session_active = false;
    store_backend(si, nullptr);
    if (!late) delete std::exchange(g_backend, nullptr);
    g_runtime_failed = true;
    runtime::report_init_failure(chosen);
}

// A decimal number with '.' as the point whatever the locale (shortcuts are
// shared between players), the whole argument; nullopt otherwise.
std::optional<float> parse_decimal(const wchar_t* arg) noexcept {
    char buf[64];
    std::size_t n = 0;
    for (; arg[n] != L'\0'; ++n) {
        if (n == sizeof(buf) || arg[n] > 0x7F) return std::nullopt;
        buf[n] = static_cast<char>(arg[n]);
    }
    float value = 0.0f;
    const auto [end, ec] = std::from_chars(buf, buf + n, value);
    if (n == 0 || ec != std::errc{} || end != buf + n) return std::nullopt;
    return value;
}

// -vrscale <s>: world scale, 1.0 outside [0.495, 1.505], for a value that is
// not a number, or with no value. Arguments are split as the shell does and
// compared whole, case-insensitively; the last -vrscale wins, so a switch
// added at the end of a shortcut overrides one already in it.
// From: FUN_1001f700 (the -vrforce* switches are handled by vr_detect).
float world_scale_from_command_line() noexcept {
    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argv) return 1.0f;
    float scale = 1.0f;
    for (int i = 1; i < argc; ++i) {
        if (::_wcsicmp(argv[i], L"-vrscale") != 0) continue;
        const std::optional<float> s = i + 1 < argc ? parse_decimal(argv[i + 1]) : std::nullopt;
        scale = (s && *s >= 0.495f && *s <= 1.505f) ? *s : 1.0f;
    }
    ::LocalFree(argv);
    return scale;
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
    g_backend->get_hmd_desc(si.desc);
    si.eye_fov[0] = si.desc.max_eye_fov[0];
    si.eye_fov[1] = si.desc.max_eye_fov[1];
    store_state(si, DeviceState::Ready);
}

// ---- Mirror (desktop window) ----------------------------------------------------

// The constant buffer of mirror.hlsl: per panel, the draw rectangle in
// window UV and the source region in stereo-texture UV, each as left, top,
// right, bottom (v down). Panel 1 is empty except for "both eyes" cropped.
struct MirrorConstants {
    float draw[2][4];
    float source[2][4];
};
static_assert(sizeof(MirrorConstants) == 0x40);

struct UvRect {
    float l, t, r, b;
};

void set_panel(MirrorConstants& c, int panel, const UvRect& draw, const UvRect& source) noexcept {
    const float d[4] = {draw.l, draw.t, draw.r, draw.b};
    const float s[4] = {source.l, source.t, source.r, source.b};
    std::memcpy(c.draw[panel], d, sizeof(d));
    std::memcpy(c.source[panel], s, sizeof(s));
}

// part / whole of a tangent, 1 when the whole is unknown.
float tan_ratio(float part, float whole) noexcept {
    return whole > 0.0f ? std::clamp(part / whole, 0.0f, 1.0f) : 1.0f;
}

// Eye e's real FOV (DefaultEyeFov) inside its half of the rendered image
// (MaxEyeFov, the FOV the eye was rendered with), in UV of the whole
// side-by-side texture: the compositor's crop bounds.
UvRect eye_crop(const StereoInterface& si, int e) noexcept {
    const auto& d = si.desc.default_eye_fov[e];
    const auto& x = si.desc.max_eye_fov[e];
    const float half = 0.5f * static_cast<float>(e);
    return {half + 0.5f * (0.5f - 0.5f * tan_ratio(d.left, x.left)),
            0.5f - 0.5f * tan_ratio(d.up, x.up),
            half + 0.5f * (0.5f + 0.5f * tan_ratio(d.right, x.right)),
            0.5f + 0.5f * tan_ratio(d.down, x.down)};
}

// What the window shows, for the current layout, window size and mirror
// settings. Head-tracked frames (`screen` false) show the eye image(s),
// filling the window width at their own aspect (a near-square eye in a
// 16:9 window is cropped top and bottom). Screen frames show the region the
// headset's virtual screen shows, eye 0's half, middle half vertically,
// fitted whole. Outside the draw area the shader paints black.
//
// Without "symmetric", each eye is cropped from the symmetric render FOV
// back to its real FOV; "both eyes" then shows eye 0's crop on the left and
// eye 1's on the right, over the union of their vertical ranges so the
// halves show the same rows and neither is stretched.
// From: FUN_1001eda0, the tail (which tests the type as 0 both, 2 left,
// otherwise right: the original's MirrorViewType integers). The original
// took the right eye's crop from the left half, drew "both eyes" cropped as
// eye 0 and its mirror image, and left the crop's height out of the draw
// height; it also showed whatever the game had bound at t0 in screen frames.
MirrorConstants mirror_constants(const StereoInterface& si, bool screen) noexcept {
    using config::MirrorViewType;
    const float tex_w = static_cast<float>(std::max(si.rt_width, 1));
    const float tex_h = static_cast<float>(std::max(si.rt_height, 1));
    const float window_aspect = si.mirror_width > 0 && si.mirror_height > 0
                                    ? static_cast<float>(si.mirror_width) / static_cast<float>(si.mirror_height)
                                    : tex_w / tex_h;
    // Pixel aspect of a source region du x dv.
    auto region_aspect = [&](float du, float dv) {
        return du > 0.0f && dv > 0.0f ? (du * tex_w) / (dv * tex_h) : window_aspect;
    };

    MirrorConstants c{};
    if (screen) {
        const UvRect source{0.0f, 0.25f, 0.5f, 0.75f};
        const float a = region_aspect(0.5f, 0.5f);
        if (a >= window_aspect) {   // bars above and below
            const float k = window_aspect / a;
            set_panel(c, 0, {0.0f, (1.0f - k) * 0.5f, 1.0f, (1.0f + k) * 0.5f}, source);
        } else {                    // bars at the sides
            const float k = a / window_aspect;
            set_panel(c, 0, {(1.0f - k) * 0.5f, 0.0f, (1.0f + k) * 0.5f, 1.0f}, source);
        }
        return c;
    }

    const auto& cfg = config::current().rendering;
    const MirrorViewType m = cfg.mirror_view_type;
    UvRect left_src{}, right_src{};
    bool both_cropped = false;
    if (cfg.symmetric_mirror_view) {
        if (m == MirrorViewType::LeftEye)       left_src = {0.0f, 0.0f, 0.5f, 1.0f};
        else if (m == MirrorViewType::BothEyes) left_src = {0.0f, 0.0f, 1.0f, 1.0f};
        else                                    left_src = {0.5f, 0.0f, 1.0f, 1.0f};
    } else if (m == MirrorViewType::BothEyes) {
        left_src  = eye_crop(si, 0);
        right_src = eye_crop(si, 1);
        const float v0 = std::min(left_src.t, right_src.t);
        const float v1 = std::max(left_src.b, right_src.b);
        left_src.t = right_src.t = v0;
        left_src.b = right_src.b = v1;
        both_cropped = true;
    } else {
        left_src = eye_crop(si, m == MirrorViewType::LeftEye ? 0 : 1);
    }

    const float du_left  = left_src.r - left_src.l;
    const float du_right = both_cropped ? right_src.r - right_src.l : 0.0f;
    const float k = window_aspect / region_aspect(du_left + du_right, left_src.b - left_src.t);
    const float top = (1.0f - k) * 0.5f, bottom = (1.0f + k) * 0.5f;
    if (both_cropped) {
        // Each half as wide as its crop's share of the summed width.
        const float split = du_left + du_right > 0.0f ? du_left / (du_left + du_right) : 0.5f;
        set_panel(c, 0, {0.0f, top, split, bottom}, left_src);
        set_panel(c, 1, {split, top, 1.0f, bottom}, right_src);
    } else {
        set_panel(c, 0, {0.0f, top, 1.0f, bottom}, left_src);
    }
    return c;
}

void release_mirror(StereoInterface& si) noexcept {
    release(si.mirror_vs);
    release(si.mirror_ps);
    release(si.mirror_cb_screen);
    release(si.mirror_cb);
    release(si.mirror_sampler);
}

bool mirror_ready(const StereoInterface& si) noexcept {
    return si.mirror_vs && si.mirror_ps && si.mirror_cb_screen && si.mirror_cb && si.mirror_sampler;
}

// (Re)creates the mirror's shaders, constants and sampler. On a failure
// none is kept and the window is cleared to black instead (end frame); VR is
// unaffected. Slot 3 only, so not per frame.
void create_mirror(StereoInterface& si, ID3D11Device* device) noexcept {
    release_mirror(si);
    const char* what = "vertex shader";
    HRESULT hr = device->CreateVertexShader(kMirrorVS, sizeof(kMirrorVS), nullptr, &si.mirror_vs);
    if (SUCCEEDED(hr)) {
        what = "pixel shader";
        hr = device->CreatePixelShader(kMirrorPS, sizeof(kMirrorPS), nullptr, &si.mirror_ps);
    }
    const D3D11_BUFFER_DESC cb_desc{sizeof(MirrorConstants), D3D11_USAGE_DEFAULT,
                                    D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
    if (SUCCEEDED(hr)) {
        what = "constant buffer";
        const MirrorConstants eyes = mirror_constants(si, false);
        const D3D11_SUBRESOURCE_DATA data{&eyes, 0, 0};
        hr = device->CreateBuffer(&cb_desc, &data, &si.mirror_cb);
    }
    if (SUCCEEDED(hr)) {
        const MirrorConstants screen = mirror_constants(si, true);
        const D3D11_SUBRESOURCE_DATA data{&screen, 0, 0};
        hr = device->CreateBuffer(&cb_desc, &data, &si.mirror_cb_screen);
    }
    if (SUCCEEDED(hr)) {
        what = "sampler";
        D3D11_SAMPLER_DESC s{};
        s.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        s.MaxAnisotropy  = 1;
        s.ComparisonFunc = D3D11_COMPARISON_NEVER;
        s.MinLOD         = -D3D11_FLOAT32_MAX;
        s.MaxLOD         = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&s, &si.mirror_sampler);
    }
    if (FAILED(hr) || !mirror_ready(si)) {
        release_mirror(si);
        diag::logf("[stereo] desktop mirror off: creating its {} failed (0x{:08X}); the window shows black, "
                   "VR not affected", what, static_cast<unsigned long>(hr));
    }
}

// ---- Device vtable (all __thiscall; see stereo_device.h) -------------------------

// [0] scalar deleting destructor. The object is static: never freed.
void __fastcall dev_release(void* self, void*, unsigned /*flags*/) noexcept {
    auto& si = device_of(self);
    if (load_state(si) == DeviceState::Running) store_state(si, DeviceState::Ready);
    if (si.hmd) store_state(si, DeviceState::None);
}

// [1] StartSensor: the game switches to stereo rendering.
void __fastcall dev_start_sensor(void* self, void*) noexcept {
    auto& si = device_of(self);
    if (load_state(si) == DeviceState::Ready) {
        store_state(si, DeviceState::Running);
        diag::log_line("[stereo] sensor started, stereo rendering on");
    }
}

// [2] StopSensor.
void __fastcall dev_stop_sensor(void* self, void*) noexcept {
    auto& si = device_of(self);
    if (load_state(si) == DeviceState::Running) store_state(si, DeviceState::Ready);
}

// [3] ConfigureRendering(stereo render target): describe the eye texture,
// create the mirror resources. If the backend cannot drive the headset with
// the game's device (no OpenXR session), the device detaches here; the
// mirror resources are still made for any call already on its way.
bool configure_rendering(StereoInterface& si, void* stereo_rt) {
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
    if (auto* backend = load_backend(si); backend && !backend->create_render_resources(device, ctx)) {
        diag::log_line("[stereo] HMD backend cannot render, running without VR");
        fail_runtime(si, runtime::last_result().chosen, true);
    }

    create_mirror(si, device);

    diag::logf("[stereo] rendering configured: {}x{} texture={}", si.rt_width, si.rt_height,
               static_cast<void*>(t0.texture));
    return true;
}

bool __fastcall dev_configure_rendering(void* self, void*, void* stereo_rt) noexcept {
    return guarded("ConfigureRendering", true, [&] { return configure_rendering(device_of(self), stereo_rt); });
}

// [4] ShutdownRendering.
bool __fastcall dev_shutdown_rendering(void* self, void*) noexcept {
    guarded("ShutdownRendering", [&] {
        if (auto* backend = load_backend(device_of(self))) backend->release_render_resources();
    });
    return true;
}

// [5] BeginFrame: poll the controllers, take the eye offsets.
void begin_frame(StereoInterface& si) {
    auto* backend = load_backend(si);
    if (!backend) return;
    backend->poll_input();
    if (backend->refresh_hmd_desc(si.desc)) {
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

void __fastcall dev_begin_frame(void* self, void*) noexcept {
    guarded("BeginFrame", [&] { begin_frame(device_of(self)); });
}

// [6] EndFrame: submit the eyes, draw the mirror, present. LibOVR's
// EndFrame presented in SDK-distortion mode, so the game does not.
void draw_mirror(StereoInterface& si, ID3D11DeviceContext* ctx, bool head_tracked) noexcept {
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) {
        // Nothing to draw into; the window keeps its last picture.
        static bool s_logged = false;
        if (!std::exchange(s_logged, true)) {
            diag::log_line("[stereo] desktop mirror: the game has no output bound at end of frame");
        }
        return;
    }
    if (!mirror_ready(si)) {
        const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        ctx->ClearRenderTargetView(rtv, black);
        rtv->Release();
        return;
    }
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
    ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    ctx->RSSetState(nullptr);
    ctx->IASetInputLayout(nullptr);
    ctx->VSSetShader(si.mirror_vs, nullptr, 0);
    ctx->PSSetShader(si.mirror_ps, nullptr, 0);
    ctx->PSSetShaderResources(1, 1, &si.eye_texture[0].srv);
    // The eyes while the game tracks the head, else the virtual screen's
    // region (menus, videos, loading).
    ctx->PSSetConstantBuffers(0, 1, head_tracked ? &si.mirror_cb : &si.mirror_cb_screen);
    ctx->PSSetSamplers(0, 1, &si.mirror_sampler);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);
    rtv->Release();  // the original leaks this reference every frame
}

void __fastcall dev_end_frame(void* self, void*) noexcept {
    auto& si = device_of(self);
    guarded("EndFrame", [&] {
        if (auto* backend = load_backend(si)) backend->submit_frame(si.eye_texture[0].texture);
    });
    auto* swap = renderer_get<IDXGISwapChain>(AiAddr::RendererSwapChain);
    auto* ctx  = renderer_get<ID3D11DeviceContext>(AiAddr::RendererContext);
    // The frame kind, decided before the request is cleared.
    draw_mirror(si, ctx, load_pose_fetched(si));
    swap->Present(0, 0);
    store_pose_fetched(si, false);
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
    guarded("GetHeadPose", [&] {
        if (auto* backend = load_backend(si)) {
            backend->head_pose(*reinterpret_cast<runtime::Matrix34*>(out), si.world_scale);
        }
    });
    store_pose_fetched(si, true);
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
void render_target_size(StereoInterface& si, runtime::HmdBackend& backend, int* w, int* h) {
    // PixelScale multiplies the runtime's recommended size, which already
    // includes SteamVR's own resolution setting. Capped so the side-by-side
    // target stays within D3D11's 16384 texture limit.
    constexpr float kMaxTextureSize = 16384.0f;
    const runtime::Sizei l1 = backend.eye_texture_size(0, 1.0f);
    const runtime::Sizei r1 = backend.eye_texture_size(1, 1.0f);
    float scale = config::current().rendering.pixel_scale;
    if (l1.w + r1.w > 0 && l1.h > 0 && r1.h > 0) {
        const float max_scale = std::min(kMaxTextureSize / static_cast<float>(l1.w + r1.w),
                                         kMaxTextureSize / static_cast<float>(std::max(l1.h, r1.h)));
        scale = std::min(scale, max_scale);
    }
    const runtime::Sizei l = backend.eye_texture_size(0, scale);
    const runtime::Sizei r = backend.eye_texture_size(1, scale);
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
                   static_cast<int>(load_state(si)), si.configured_width, si.configured_height);
    }
    *w = width;
    *h = height;
}

int* __fastcall dev_render_target_size(void* self, void*, int* w, int* h) noexcept {
    auto& si = device_of(self);
    if (auto* backend = load_backend(si)) {
        guarded("GetRenderTargetSize", [&] { render_target_size(si, *backend, w, h); });
    }
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
    guarded("RecenterAndGetHeadPose", [&] {
        if (auto* backend = load_backend(si)) {
            backend->head_yaw_pose(*reinterpret_cast<runtime::Matrix34*>(out), si.world_scale);
        }
    });
    return out;
}

// ---- Manager vtable ---------------------------------------------------------------

// [0] scalar deleting destructor: static object.
void __fastcall mgr_release(void*, void*, unsigned) noexcept {}

// [1] OnEvent(Event*). Event 0x18 starts the sensor and switches four
// graphics options off/on.
void __fastcall mgr_on_event(StereoManager* self, void*, const std::int32_t* event) noexcept {
    StereoInterface* device = load_device(*self);
    if (*event != 0x18 || !device) return;
    reinterpret_cast<void (__fastcall*)(void*, void*)>(device->vtable[1])(device, nullptr);

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

namespace {

void create_device_impl(StereoManager* self) {
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
    StereoInterface* device = load_state(si) != DeviceState::None ? &si : nullptr;
    store_device(*self, device);
    integration::g_vr_session_active = device != nullptr;
    diag::logf("[stereo] CreateDevice{}: {}", first ? "" : " again", device ? "HMD attached" : "no HMD");
    if (first && device && load_state(si) == DeviceState::Ready) {
        dev_recenter_pose(&si, nullptr, self->reference_pose);
        // The headset may be lying on the desk at launch: take the
        // reference again once it is worn.
        if (auto* backend = load_backend(si); backend && !backend->headset_in_use()) request_recenter();
    }
}

} // namespace

void __fastcall create_device(StereoManager* self, void*, int enable) noexcept {
    if (enable != 1) return;
    guarded("CreateDevice", [&] { create_device_impl(self); });
}

int* __fastcall get_render_target_size(StereoManager* self, void*, int* w, int* h) noexcept {
    StereoInterface* si = load_device(*self);
    if (!si) return h;
    si->mirror_width  = *w;
    si->mirror_height = *h;
    return reinterpret_cast<int* (__fastcall*)(void*, void*, int*, int*)>(si->vtable[16])(si, nullptr, w, h);
}

int* __fastcall get_hmd_resolution(StereoManager*, void*, int* /*w*/, int* h) noexcept { return h; }

} // namespace mothervr::stereo
