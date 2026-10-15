#pragma once

#include "game/addresses.h"
#include "game/constants.h"
#include "game/stereo_extension.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

// The stereo manager and device the game is given in place of its own:
// their vtables, initial values, the atomic access to the fields other
// threads read, and the calls through the device's own slots (02 §3, §5).
namespace omvr::game::stereo {

// The device's vtable (02 §5): 21 slots of the game's layout and the mod's
// extra slot 21. Slots 12 to 14 are the game's own functions.
enum class DeviceSlot : std::size_t {
    Release             = 0,
    StartSensor         = 1,
    StopSensor          = 2,
    ConfigureRendering  = 3,
    ShutdownRendering   = 4,
    BeginFrame          = 5,
    EndFrame            = 6,
    BeginEyeRender      = 7,
    EndEyeRender        = 8,
    IncrementFrameIndex = 9,
    DevicePresents      = 10,
    HeadPose            = kDeviceHeadPoseSlot,
    GameEyeTransform    = 12,
    GameFovAspect       = 13,
    GameEyeScaleOffset  = 14,
    EyeViewport         = 15,
    RenderTargetSize    = 16,
    HmdResolution       = 17,
    DisplayName         = 18,
    PositionTracked     = 19,
    Unused              = 20,
    RecenterPose        = kDeviceReferencePoseSlot,
};
inline constexpr std::size_t kDeviceSlotCount = std::to_underlying(DeviceSlot::RecenterPose) + 1;

// The manager's vtable (02 §3.2).
enum class ManagerSlot : std::size_t {
    Release  = 0,
    OnEvent  = 1,
    OnEvent2 = 2,
};
inline constexpr std::size_t kManagerSlotCount = std::to_underlying(ManagerSlot::OnEvent2) + 1;

using DeviceSlots  = std::array<void*, kDeviceSlotCount>;
using ManagerSlots = std::array<void*, kManagerSlotCount>;

// The slots' types (02 §3.2, §5), and the manager methods whose call sites
// the installer redirects (02 §6.1, §6.2). The game's are __thiscall; the
// mod's replacements are __fastcall(self, edx, ...) with the same stack
// arguments, so the callee pops what the game expects.
namespace abi {
using Release            = void (__fastcall*)(void* self, void* edx, unsigned flags);         // ret 4
using NoArgs             = void (__fastcall*)(void* self, void* edx);                         // ret 0
using BoolNoArgs         = bool (__fastcall*)(void* self, void* edx);                         // ret 0, AL
using ConfigureRendering = bool (__fastcall*)(void* self, void* edx, void* stereo_rt);        // ret 4, AL
using EyeRender          = void (__fastcall*)(void* self, void* edx, int eye);                // ret 4
using Pose               = float* (__fastcall*)(void* self, void* edx, float* out);           // ret 4, EAX
using EyeViewport        = void (__fastcall*)(void* self, void* edx, int eye, int half_width, int height,
                                       int* left, int* top, int* right, int* bottom);  // ret 0x1C
using SizeQuery          = int* (__fastcall*)(void* self, void* edx, int* w, int* h);         // ret 8
using DisplayName        = char* (__fastcall*)(void* self, void* edx, char* buf, int size);  // ret 8, EAX
using OnEvent            = void (__fastcall*)(StereoManager* self, void* edx, const std::int32_t* event);  // ret 4
using OnEvent2           = bool (__fastcall*)(void* self, void* edx, const void* setting, int value);   // ret 8
using CreateDevice       = void (__fastcall*)(StereoManager* self, void* edx, int enable);      // ret 4
using ManagerSizeQuery   = int* (__fastcall*)(StereoManager* self, void* edx, int* w, int* h);  // ret 8
// The manager's per-frame update and its mode stack (05 §3, §4.4).
using Update   = int (__fastcall*)(StereoManager* self, void* edx, float dt);      // ret 4
using PushMode = void (__fastcall*)(StereoManager* self, void* edx, int mode);     // ret 4
using PopMode  = void (__fastcall*)(StereoManager* self, void* edx);               // ret 0
// Matrix44* BuildUiQuad(Matrix44* out, float size, float inv_aspect, float
// dist), __cdecl: the caller copies the result through the returned
// pointer (05 §4.1).
using BuildUiQuad = float* (__cdecl*)(float* out, float size, float inv_aspect, float dist);
} // namespace abi

// The device state is read on the game's job threads (the cull hooks) and
// the render thread while the main thread changes it, and a late detach
// (slot 3, render thread) clears it: writes, and reads off the main thread,
// go through atomic_ref. Relaxed: a value one frame stale is harmless. The
// layout is the game's and stays as it is.
inline DeviceState load_state(StereoInterface& si) noexcept {
    return std::atomic_ref(si.state).load(std::memory_order_relaxed);
}
inline void store_state(StereoInterface& si, DeviceState s) noexcept {
    std::atomic_ref(si.state).store(s, std::memory_order_relaxed);
}

// As load_state / store_state, for the attached device.
inline StereoInterface* load_device(StereoManager& m) noexcept {
    return std::atomic_ref(m.device).load(std::memory_order_relaxed);
}
inline void store_device(StereoManager& m, StereoInterface* device) noexcept {
    std::atomic_ref(m.device).store(device, std::memory_order_relaxed);
}

// Builds both vtables (slots 12 to 14 of the device point into AI.exe) and
// the initial field values of `m` (02 §3.3, §4). Call once from the hook
// installer, before the game is handed the manager.
// From: FUN_10003510 + FUN_10003640.
void initialize(const AddressMap& addresses, StereoManager& m, const DeviceSlots& device,
                const ManagerSlots& manager) noexcept;

// A plausible LibOVR 0.4 descriptor header for the game: a DK2 (02 §4: the
// game reads none of it outside the replaced slots).
void describe_as_dk2(StereoInterface& si) noexcept;

// The calls the mod makes through the device's own vtable, so a replaced
// slot runs as the game would run it.
// Slot 1, StartSensor.
void start_sensor(StereoInterface& si) noexcept;
// Slot 16, GetRenderTargetSize(int* w, int* h).
std::int32_t* render_target_size(StereoInterface& si, std::int32_t* w, std::int32_t* h) noexcept;
// Slot 21, RecenterAndGetHeadPose: takes the reference pose into the
// manager's reference_pose (02 §6.4).
void recenter(StereoManager& m, StereoInterface& si) noexcept;

} // namespace omvr::game::stereo
