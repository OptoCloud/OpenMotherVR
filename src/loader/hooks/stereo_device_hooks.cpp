// The stereo device's and manager's vtable slots, and the manager calls the
// game makes through redirected call sites.
//
// From: the StereoInterface vtable 0x1005a5c0 (FUN_1001ed50 .. FUN_1001f300)
// and the StereoManager vtable 0x1005a61c (FUN_10020230, FUN_10020290),
// FUN_1001f6a0 (CreateDevice), FUN_100202a0 (GetRenderTargetSize hook).
// Analysis: dumps/06a_stereo_interface.md.

#include "loader/hooks/stereo_device_hooks.h"

#include "core/stereo/render_target.h"
#include "core/stereo/sensor_start.h"
#include "game/stereo/renderer.h"
#include "loader/context.h"
#include "loader/hooks/guard.h"
#include "platform/d3d11/handles.h"
#include "platform/d3d11/render/mirror_renderer.h"
#include "util/math/matrix.h"

#include <d3d11.h>
#include <dxgi.h>

#include <cstdint>

namespace omvr::loader::hooks {

namespace {

using core::stereo::render_target_size;
using game::AiAddr;
using game::DeviceState;
using game::StereoInterface;
using game::StereoManager;
using game::load_backend;
using game::load_pose_fetched;
using game::store_pose_fetched;
using game::stereo::load_device;
using game::stereo::load_state;
using game::stereo::renderer_get;
using game::stereo::store_state;

StereoInterface& device_of(void* self) noexcept { return *static_cast<StereoInterface*>(self); }

platform::d3d11::MirrorObjects mirror_objects(StereoInterface& si) noexcept {
    return {si.mirror_vs, si.mirror_ps, si.mirror_cb_screen, si.mirror_cb, si.mirror_sampler};
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


bool __fastcall dev_configure_rendering(void* self, void*, void* stereo_rt) noexcept {
    return guarded("ConfigureRendering", true, [&] { return context().stereo.configure_rendering(device_of(self), stereo_rt); });
}

// [4] ShutdownRendering.
bool __fastcall dev_shutdown_rendering(void* self, void*) noexcept {
    guarded("ShutdownRendering", [&] {
        if (auto* backend = load_backend(device_of(self))) backend->release_render_resources();
    });
    return true;
}


void __fastcall dev_begin_frame(void* self, void*) noexcept {
    guarded("BeginFrame", [&] { context().stereo.begin_frame(device_of(self)); });
}


// [6] EndFrame: submit the eyes, draw the mirror, present. LibOVR's
// EndFrame presented in SDK-distortion mode, so the game does not.
void __fastcall dev_end_frame(void* self, void*) noexcept {
    auto& si = device_of(self);
    guarded("EndFrame", [&] {
        if (auto* backend = load_backend(si)) backend->submit_frame(platform::d3d11::gpu_texture(si.eye_texture[0].texture));
    });
    auto* swap = renderer_get<IDXGISwapChain>(AiAddr::RendererSwapChain);
    auto* ctx  = renderer_get<ID3D11DeviceContext>(AiAddr::RendererContext);
    // The frame kind, decided before the request is cleared.
    platform::d3d11::draw_mirror(mirror_objects(si), ctx, &si.eye_texture[0].srv, load_pose_fetched(si));
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
            backend->head_pose(*reinterpret_cast<util::Matrix34*>(out), si.world_scale);
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
            backend->head_yaw_pose(*reinterpret_cast<util::Matrix34*>(out), si.world_scale);
            if (!backend->recenter_taken()) context().update.request_recenter();
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
    core::stereo::apply_sensor_start_options();
}


// [2] OnEvent2(Event*, int): always handled.
bool __fastcall mgr_on_event2(void*, void*, const void*, int) noexcept { return true; }

template <class Fn>
void* as_slot(Fn fn) noexcept { return reinterpret_cast<void*>(fn); }


} // namespace

game::stereo::DeviceSlots device_slots() noexcept {
    return {as_slot(&dev_release),
            as_slot(&dev_start_sensor),
            as_slot(&dev_stop_sensor),
            as_slot(&dev_configure_rendering),
            as_slot(&dev_shutdown_rendering),
            as_slot(&dev_begin_frame),
            as_slot(&dev_end_frame),
            as_slot(&dev_eye_render_nop),
            as_slot(&dev_eye_render_nop),
            as_slot(&dev_increment_frame),
            as_slot(&dev_is_ready),
            as_slot(&dev_head_pose),
            nullptr,   // [12..14] the game's own, from the address table
            nullptr,
            nullptr,
            as_slot(&dev_eye_viewport),
            as_slot(&dev_render_target_size),
            as_slot(&dev_hmd_resolution),
            as_slot(&dev_display_name),
            as_slot(&dev_position_tracked),
            as_slot(&dev_unused),
            as_slot(&dev_recenter_pose)};
}

game::stereo::ManagerSlots manager_slots() noexcept {
    return {as_slot(&mgr_release), as_slot(&mgr_on_event), as_slot(&mgr_on_event2)};
}

void __fastcall create_device(StereoManager* self, void*, int enable) noexcept {
    if (enable != 1) return;
    guarded("CreateDevice", [&] { context().stereo.create_device(self); });
}

int* __fastcall get_render_target_size(StereoManager* self, void*, int* w, int* h) noexcept {
    StereoInterface* si = load_device(*self);
    if (!si) return h;
    si->mirror_width  = *w;
    si->mirror_height = *h;
    return reinterpret_cast<int* (__fastcall*)(void*, void*, int*, int*)>(si->vtable[16])(si, nullptr, w, h);
}

int* __fastcall get_hmd_resolution(StereoManager*, void*, int* /*w*/, int* h) noexcept { return h; }

} // namespace omvr::loader::hooks
