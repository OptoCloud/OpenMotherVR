// The stereo device's and manager's vtable slots, and the manager calls the
// game makes through redirected call sites (02 §3.2, §5, §6).
//
// From: the StereoInterface vtable 0x1005a5c0 (FUN_1001ed50 .. FUN_1001f300)
// and the StereoManager vtable 0x1005a61c (FUN_10020230, FUN_10020290),
// FUN_1001f6a0 (CreateDevice), FUN_100202a0 (GetRenderTargetSize hook).
// Analysis: dumps/06a_stereo_interface.md.

#include "loader/hooks/stereo_device_hooks.h"

#include "core/stereo/sensor_start.h"
#include "game/stereo/graphics_settings.h"
#include "game/stereo/renderer.h"
#include "loader/context.h"
#include "loader/hooks/guard.h"
#include "loader/hooks/hand_hooks.h"
#include "loader/hooks/haptic_hooks.h"
#include "platform/d3d11/handles.h"
#include "platform/d3d11/render/mirror_renderer.h"
#include "util/math/matrix.h"

#include <d3d11.h>
#include <dxgi.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace omvr::loader::hooks {

namespace {

using core::stereo::PixelSize;
using game::AiAddr;
using game::DeviceState;
using game::load_backend;
using game::load_pose_fetched;
using game::StereoInterface;
using game::StereoManager;
using game::store_pose_fetched;
using game::stereo::DeviceSlot;
using game::stereo::load_device;
using game::stereo::load_state;
using game::stereo::ManagerSlot;
using game::stereo::renderer_get;
using game::stereo::renderer_rt;
using game::stereo::store_state;
namespace abi = game::stereo::abi;

constexpr std::string_view kSensorStartedLog = "[stereo] sensor started, stereo rendering on";

// The device presents the game's swapchain itself (02 §5 slot 6): without
// waiting for vsync (the runtime paces the frames), no flags.
constexpr UINT kPresentSyncInterval = 0;
constexpr UINT kPresentFlags        = 0;

// Slot 15's eye 1 is the right half.
constexpr int kRightEye = 1;

StereoInterface& device_of(void* self) noexcept { return *static_cast<StereoInterface*>(self); }

// The 3x4 head pose the game hands in, as the runtime port writes it.
util::Matrix34& pose_out(float* out) noexcept { return *reinterpret_cast<util::Matrix34*>(out); }

// The stereo texture's real size, nullopt without one (03 §4.3).
std::optional<PixelSize> texture_size(ID3D11Texture2D* texture) noexcept {
    if (!texture) return std::nullopt;
    D3D11_TEXTURE2D_DESC td{};
    texture->GetDesc(&td);
    return PixelSize{static_cast<std::int32_t>(td.Width), static_cast<std::int32_t>(td.Height)};
}

// The game's stereo render target `stereo_rt` as the eye texture (03 §4.4);
// a null target keeps the previous one.
void take_stereo_target(const game::AddressMap& addresses, StereoInterface& si, void* stereo_rt) noexcept {
    if (!stereo_rt) return;
    auto& t0         = si.eye_texture[0];
    const auto index = game::stereo::render_target_index(stereo_rt);
    t0.texture       = renderer_rt<ID3D11Texture2D>(addresses, AiAddr::RendererRtTexture, index);
    t0.srv           = renderer_rt<ID3D11ShaderResourceView>(addresses, AiAddr::RendererRtSrv, index);
}

// Spec 22 §4.3, §5: the comfort vignette and the snap-turn fade, drawn into
// the eyes before the runtime takes them (OpenVR's submit, OpenXR's copy)
// and before the mirror samples them. Head-tracked frames only. Nothing to
// draw costs one comparison. Render thread.
void draw_comfort(Context& ctx, StereoInterface& si) noexcept {
    const auto& fov      = si.eye_fov[0]; // symmetric (03 §3.2): one eye gives both
    const auto constants = ctx.comfort.frame(ctx.clock.now(), load_pose_fetched(si),
                                             std::max(fov.left, fov.right), std::max(fov.up, fov.down));
    if (!constants) return;
    auto* dc = renderer_get<ID3D11DeviceContext>(ctx.addresses, AiAddr::RendererContext);
    ctx.vignette.draw(platform::d3d11::gpu_context(dc),
                      platform::d3d11::gpu_texture(si.eye_texture[0].texture), *constants);
}

// ---- Device vtable (all __thiscall, 02 §5) ----------------------------------------

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
        context().log.line(kSensorStartedLog);
    }
}

// [2] StopSensor.
void __fastcall dev_stop_sensor(void* self, void*) noexcept {
    auto& si = device_of(self);
    if (load_state(si) == DeviceState::Running) store_state(si, DeviceState::Ready);
}

// [3] ConfigureRendering(stereo render target). Render thread.
bool __fastcall dev_configure_rendering(void* self, void*, void* stereo_rt) noexcept {
    return guarded("ConfigureRendering", true, [&] {
        auto& ctx = context();
        auto& si  = device_of(self);
        take_stereo_target(ctx.addresses, si, stereo_rt);
        const auto size = texture_size(si.eye_texture[0].texture);
        auto* device    = renderer_get<ID3D11Device>(ctx.addresses, AiAddr::RendererDevice);
        auto* dc        = renderer_get<ID3D11DeviceContext>(ctx.addresses, AiAddr::RendererContext);
        // Spec 22 §15: the comfort pass's resources follow the target.
        ctx.vignette.configure(platform::d3d11::gpu_device(device),
                               platform::d3d11::gpu_texture(si.eye_texture[0].texture));
        return ctx.stereo.configure_rendering(si, size, platform::d3d11::gpu_device(device),
                                              platform::d3d11::gpu_context(dc), ctx.mirror);
    });
}

// [4] ShutdownRendering.
bool __fastcall dev_shutdown_rendering(void* self, void*) noexcept {
    guarded("ShutdownRendering", [&] {
        if (auto* backend = load_backend(device_of(self))) backend->release_render_resources();
    });
    return true;
}

// [5] BeginFrame. Render thread.
void __fastcall dev_begin_frame(void* self, void*) noexcept {
    guarded("BeginFrame", [&] { context().stereo.begin_frame(device_of(self)); });
}

// [6] EndFrame: submit the eyes, draw the mirror, present. LibOVR's
// EndFrame presented in SDK-distortion mode, so the game does not. Render
// thread.
void __fastcall dev_end_frame(void* self, void*) noexcept {
    auto& ctx = context();
    auto& si  = device_of(self);
    guarded("EndFrame", [&] {
        if (auto* backend = load_backend(si)) {
            draw_comfort(ctx, si);
            backend->submit_frame(platform::d3d11::gpu_texture(si.eye_texture[0].texture));
        }
    });
    auto* swap = renderer_get<IDXGISwapChain>(ctx.addresses, AiAddr::RendererSwapChain);
    auto* dc   = renderer_get<ID3D11DeviceContext>(ctx.addresses, AiAddr::RendererContext);
    // The frame kind, decided before the request is cleared.
    ctx.mirror.draw(platform::d3d11::gpu_context(dc), load_pose_fetched(si));
    swap->Present(kPresentSyncInterval, kPresentFlags);
    store_pose_fetched(si, false);
    // 22 §13: a VR-off request takes effect between frames; the game's
    // targets then return to the window's size.
    if (ctx.stereo.turn_off_if_requested(si)) ctx.display_reset.reset_after_vr_off();
}

// [7] BeginEyeRender / [8] EndEyeRender: nothing to do.
void __fastcall dev_eye_render_nop(void*, void*, int /*eye*/) noexcept {}

// [9] IncrementFrameIndex.
void __fastcall dev_increment_frame(void* self, void*) noexcept { ++device_of(self).frame_index; }

// [10] The device presents: true, so the game does not present the back
// buffer itself as well (02 §5).
bool __fastcall dev_presents(void*, void*) noexcept { return true; }

// [11] GetHeadPose(float out[12]). Game thread.
float* __fastcall dev_head_pose(void* self, void*, float* out) noexcept {
    auto& si = device_of(self);
    guarded("GetHeadPose", [&] {
        if (auto* backend = load_backend(si)) {
            backend->head_pose(pose_out(out), si.world_scale);
            hands_on_head_pose(si, *backend, pose_out(out)); // 21 §3.4: the hands of this pose
            // 21 §14: the health and the firing of this frame.
            haptics_on_head_pose();
            context().head_clip_pivot.on_head_pose(pose_out(out));
        }
    });
    store_pose_fetched(si, true);
    return out;
}

// [15] GetEyeViewportRect(eye, half_width, height, &left, &top, &right,
// &bottom) for a target of the given size (07 §4.4). The game's version
// ignores the size and returns the main target's eye halves, which is wrong
// for the half-size reflection target once that is rendered per eye.
void __fastcall dev_eye_viewport(void*, void*, int eye, int half_width, int height, int* left, int* top,
                                 int* right, int* bottom) noexcept {
    const int x = eye == kRightEye ? half_width : 0;
    *left       = x;
    *top        = 0;
    *right      = x + half_width;
    *bottom     = height;
}

// [16] GetRenderTargetSize(int* w, int* h).
int* __fastcall dev_render_target_size(void* self, void*, int* w, int* h) noexcept {
    auto& si = device_of(self);
    if (auto* backend = load_backend(si)) {
        guarded("GetRenderTargetSize", [&] {
            const PixelSize size = context().stereo.render_target_size(si, *backend);
            *w                   = size.width;
            *h                   = size.height;
        });
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
// game's recenter call sites are redirected here from slot 11. Game thread.
//
// A recenter that met an invalid head pose keeps the previous reference
// (03 section 2.1, 09 section 5.5); the retake is queued here, so the chord,
// CreateDevice and the game's own recenter sites are all taken again once
// the headset is worn and tracked.
float* __fastcall dev_recenter_pose(void* self, void*, float* out) noexcept {
    auto& si = device_of(self);
    guarded("RecenterAndGetHeadPose", [&] {
        if (auto* backend = load_backend(si)) {
            backend->head_yaw_pose(pose_out(out), si.world_scale);
            context().head_clip_pivot.on_reference(pose_out(out));
            if (!backend->recenter_taken()) context().update.request_recenter();
        }
    });
    return out;
}

// ---- Manager vtable (02 §3.2) ------------------------------------------------------

// [0] scalar deleting destructor: static object.
void __fastcall mgr_release(void*, void*, unsigned) noexcept {}

// [1] OnEvent(Event*): the stereo mode setting starts the sensor and
// switches graphics settings for stereo (02 §6.5).
void __fastcall mgr_on_event(StereoManager* self, void*, const std::int32_t* event) noexcept {
    StereoInterface* device = load_device(*self);
    if (!core::stereo::starts_sensor(*event) || !device) return;
    game::stereo::start_sensor(*device);
    for (const auto& change : core::stereo::kSensorStartChanges) {
        game::stereo::set_graphics_option(context().addresses, change.option, change.value);
    }
}

// [2] OnEvent2(Event*, int): always handled.
bool __fastcall mgr_on_event2(void*, void*, const void*, int) noexcept { return true; }

// Each replacement has the type of the slot it fills.
static_assert(std::is_convertible_v<decltype(&dev_release), abi::Release>);
static_assert(std::is_convertible_v<decltype(&dev_start_sensor), abi::NoArgs>);
static_assert(std::is_convertible_v<decltype(&dev_stop_sensor), abi::NoArgs>);
static_assert(std::is_convertible_v<decltype(&dev_configure_rendering), abi::ConfigureRendering>);
static_assert(std::is_convertible_v<decltype(&dev_shutdown_rendering), abi::BoolNoArgs>);
static_assert(std::is_convertible_v<decltype(&dev_begin_frame), abi::NoArgs>);
static_assert(std::is_convertible_v<decltype(&dev_end_frame), abi::NoArgs>);
static_assert(std::is_convertible_v<decltype(&dev_eye_render_nop), abi::EyeRender>);
static_assert(std::is_convertible_v<decltype(&dev_increment_frame), abi::NoArgs>);
static_assert(std::is_convertible_v<decltype(&dev_presents), abi::BoolNoArgs>);
static_assert(std::is_convertible_v<decltype(&dev_head_pose), abi::Pose>);
static_assert(std::is_convertible_v<decltype(&dev_eye_viewport), abi::EyeViewport>);
static_assert(std::is_convertible_v<decltype(&dev_render_target_size), abi::SizeQuery>);
static_assert(std::is_convertible_v<decltype(&dev_hmd_resolution), abi::SizeQuery>);
static_assert(std::is_convertible_v<decltype(&dev_display_name), abi::DisplayName>);
static_assert(std::is_convertible_v<decltype(&dev_position_tracked), abi::BoolNoArgs>);
static_assert(std::is_convertible_v<decltype(&dev_unused), abi::NoArgs>);
static_assert(std::is_convertible_v<decltype(&dev_recenter_pose), abi::Pose>);
static_assert(std::is_convertible_v<decltype(&mgr_release), abi::Release>);
static_assert(std::is_convertible_v<decltype(&mgr_on_event), abi::OnEvent>);
static_assert(std::is_convertible_v<decltype(&mgr_on_event2), abi::OnEvent2>);

// A replacement's address, as the game's vtable holds it.
template <class Fn>
void* as_slot(Fn fn) noexcept {
    return reinterpret_cast<void*>(fn);
}

} // namespace

static_assert(std::is_convertible_v<decltype(&create_device), abi::CreateDevice>);
static_assert(std::is_convertible_v<decltype(&get_render_target_size), abi::ManagerSizeQuery>);
static_assert(std::is_convertible_v<decltype(&get_hmd_resolution), abi::ManagerSizeQuery>);

game::stereo::DeviceSlots device_slots() noexcept {
    game::stereo::DeviceSlots slots{}; // slots 12 to 14: the game's own, from the address table
    const auto set = [&slots](DeviceSlot slot, void* fn) { slots[std::to_underlying(slot)] = fn; };
    set(DeviceSlot::Release, as_slot(&dev_release));
    set(DeviceSlot::StartSensor, as_slot(&dev_start_sensor));
    set(DeviceSlot::StopSensor, as_slot(&dev_stop_sensor));
    set(DeviceSlot::ConfigureRendering, as_slot(&dev_configure_rendering));
    set(DeviceSlot::ShutdownRendering, as_slot(&dev_shutdown_rendering));
    set(DeviceSlot::BeginFrame, as_slot(&dev_begin_frame));
    set(DeviceSlot::EndFrame, as_slot(&dev_end_frame));
    set(DeviceSlot::BeginEyeRender, as_slot(&dev_eye_render_nop));
    set(DeviceSlot::EndEyeRender, as_slot(&dev_eye_render_nop));
    set(DeviceSlot::IncrementFrameIndex, as_slot(&dev_increment_frame));
    set(DeviceSlot::DevicePresents, as_slot(&dev_presents));
    set(DeviceSlot::HeadPose, as_slot(&dev_head_pose));
    set(DeviceSlot::EyeViewport, as_slot(&dev_eye_viewport));
    set(DeviceSlot::RenderTargetSize, as_slot(&dev_render_target_size));
    set(DeviceSlot::HmdResolution, as_slot(&dev_hmd_resolution));
    set(DeviceSlot::DisplayName, as_slot(&dev_display_name));
    set(DeviceSlot::PositionTracked, as_slot(&dev_position_tracked));
    set(DeviceSlot::Unused, as_slot(&dev_unused));
    set(DeviceSlot::RecenterPose, as_slot(&dev_recenter_pose));
    return slots;
}

game::stereo::ManagerSlots manager_slots() noexcept {
    game::stereo::ManagerSlots slots{};
    slots[std::to_underlying(ManagerSlot::Release)]  = as_slot(&mgr_release);
    slots[std::to_underlying(ManagerSlot::OnEvent)]  = as_slot(&mgr_on_event);
    slots[std::to_underlying(ManagerSlot::OnEvent2)] = as_slot(&mgr_on_event2);
    return slots;
}

void __fastcall create_device(StereoManager* self, void*, int enable) noexcept {
    // CreateDevice(1) asks for the headset (02 §6.1); anything else does nothing.
    if (enable != 1) return;
    guarded("CreateDevice", [&] { context().stereo.create_device(*self); });
}

int* __fastcall get_render_target_size(StereoManager* self, void*, int* w, int* h) noexcept {
    StereoInterface* si = load_device(*self);
    if (!si) return h;
    si->mirror_width  = *w;
    si->mirror_height = *h;
    return game::stereo::render_target_size(*si, w, h);
}

int* __fastcall get_hmd_resolution(StereoManager*, void*, int* /*w*/, int* h) noexcept { return h; }

} // namespace omvr::loader::hooks
