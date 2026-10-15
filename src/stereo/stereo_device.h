#pragma once

#include "runtime/hmd_backend.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11Buffer;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;

// The game's stereo objects, replaced by ours.
//
// Alien: Isolation ships a hidden LibOVR 0.4-era stereo path: a stereo
// manager (game: new(0xb0), vtable 0x13E57BC) that owns a stereo device
// (new(0x1f4), vtable 0x13E5418, 21 methods). MotherVR hands the game one
// static object of its own instead: the manager, with the device embedded at
// +0xB0. Both keep the game's field layout because game code reads them
// directly, and four device methods stay the game's own (vtable slots 12-15)
// and read the device through those offsets.
//
// From: MotherVR::StereoManager (DAT_1006ac18, vtable 0x1005a61c, init
// FUN_10003510) and MotherVR::StereoInterface (DAT_1006acc8, vtable
// 0x1005a5c0, ctor FUN_10003640). Analysis: dumps/06a_stereo_interface.md.

namespace mothervr::stereo {

struct Recti {
    std::int32_t x, y, w, h;
};

// LibOVR 0.4.0 ovrEyeRenderDesc. Only view_adjust is used.
struct EyeRenderDesc {
    std::int32_t     eye;
    runtime::FovPort fov;
    Recti            distorted_viewport;
    float            pixels_per_tan[2];
    float            view_adjust[3];      // eye offset from the head, scaled
};
static_assert(sizeof(EyeRenderDesc) == 0x38);

// LibOVR 0.4.0 ovrD3D11Texture.
struct EyeTexture {
    std::int32_t              api;        // 5 = D3D11
    runtime::Sizei            size;
    Recti                     viewport;
    struct ID3D11Texture2D*   texture;
    ID3D11ShaderResourceView* srv;
    std::uintptr_t            reserved[6];
};
static_assert(sizeof(EyeTexture) == 0x3C);

struct Posef {
    float orientation[4];
    float position[3];
};

enum class DeviceState : std::int32_t {
    None    = 0,   // no HMD
    Ready   = 1,   // initialised
    Running = 2,   // "sensor started": the game renders in stereo
};

// The stereo device. +0x000..+0x1F3 is the game's layout.
struct StereoInterface {
    void* const*     vtable;              // +0x000
    DeviceState      state;               // +0x004
    std::uint32_t    frame_index;         // +0x008
    void*            hmd;                 // +0x00C  game: ovrHmd. Always null
    runtime::HmdDesc desc;                // +0x010
    Recti            viewport[2];         // +0x08C  eye halves of the render target
    runtime::FovPort eye_fov[2];          // +0x0AC  the FOV the game renders
    EyeRenderDesc    eye_desc[2];         // +0x0CC
    EyeTexture       eye_texture[2];      // +0x13C
    Posef            eye_pose[2];         // +0x1B4  unused
    std::int32_t     rt_width;            // +0x1EC  both eyes side by side
    std::int32_t     rt_height;           // +0x1F0
    // ---- MotherVR ----
    runtime::HmdBackend* backend;         // +0x1F4
    std::int32_t     runtime_id;          // +0x1F8  0 none, 1 Oculus, 2 OpenVR
    std::int32_t     handle_seed;         // +0x1FC
    std::uint32_t    unused_200;          // +0x200
    float            world_scale;         // +0x204  -vrscale
    std::int32_t     pose_fetched;        // +0x208  head pose read since the last submit
    std::int32_t     mirror_width;        // +0x20C  desktop window size
    std::int32_t     mirror_height;       // +0x210
    std::int32_t     configured_width;    // +0x214  size of the eye texture in use
    std::int32_t     configured_height;   // +0x218  (unused in the original)
    // The mirror (stereo_device.cpp, mirror.hlsl). One shader for every
    // mode; the frame kind picks the constant buffer. The original had a
    // second pixel shader at +0x224 for screen frames.
    ID3D11VertexShader* mirror_vs;        // +0x21C
    ID3D11PixelShader*  mirror_ps;        // +0x220
    ID3D11Buffer*       mirror_cb_screen; // +0x224  screen frames
    ID3D11Buffer*       mirror_cb;        // +0x228  head-tracked frames
    ID3D11SamplerState* mirror_sampler;   // +0x22C
};
static_assert(offsetof(StereoInterface, desc) == 0x10);
static_assert(offsetof(StereoInterface, viewport) == 0x8C);
static_assert(offsetof(StereoInterface, eye_fov) == 0xAC);
static_assert(offsetof(StereoInterface, eye_desc) == 0xCC);
static_assert(offsetof(StereoInterface, eye_desc[0].view_adjust) == 0xF8);
static_assert(offsetof(StereoInterface, eye_desc[1].view_adjust) == 0x130);
static_assert(offsetof(StereoInterface, eye_texture) == 0x13C);
static_assert(offsetof(StereoInterface, eye_texture[0].texture) == 0x158);
static_assert(offsetof(StereoInterface, eye_texture[0].srv) == 0x15C);
static_assert(offsetof(StereoInterface, eye_pose) == 0x1B4);
static_assert(offsetof(StereoInterface, rt_width) == 0x1EC);
static_assert(offsetof(StereoInterface, backend) == 0x1F4);
static_assert(offsetof(StereoInterface, world_scale) == 0x204);
static_assert(offsetof(StereoInterface, mirror_width) == 0x20C);
static_assert(offsetof(StereoInterface, mirror_vs) == 0x21C);
static_assert(offsetof(StereoInterface, mirror_sampler) == 0x22C);
static_assert(sizeof(StereoInterface) == 0x230);

// The device state is read on the game's job threads (the cull hooks) while
// the main thread changes it: writes, and reads off the main thread, go
// through atomic_ref. Relaxed: a value one frame stale is harmless. The
// layout is the game's and stays as it is.
inline DeviceState load_state(StereoInterface& si) noexcept {
    return std::atomic_ref(si.state).load(std::memory_order_relaxed);
}
inline void store_state(StereoInterface& si, DeviceState s) noexcept {
    std::atomic_ref(si.state).store(s, std::memory_order_relaxed);
}

// The stereo manager. +0x00..+0xAF is the game's layout.
struct StereoManager {
    void* const*     vtable;              // +0x000
    StereoInterface* device;              // +0x004  null when there is no HMD
    void*            stereo_rt;           // +0x008  the game's stereo render target
    std::uint8_t     game_0c[0x14];       // +0x00C
    float            game_matrix[16];     // +0x020
    std::int32_t     mode;                // +0x060
    float            game_64;             // +0x064  scene visibility
    std::uint8_t     game_68[4];          // +0x068
    std::int32_t     stack_top;           // +0x06C  game: mode-stack top, kept equal to mode
    std::uint8_t     game_70[8];          // +0x070
    float            reference_pose[12];  // +0x078  recenter pose, 3x4
    std::uint8_t     game_a8;             // +0x0A8
    std::uint8_t     game_a9[3];
    float            fade;                // +0x0AC
    StereoInterface  si;                  // +0x0B0
};
static_assert(offsetof(StereoManager, mode) == 0x60);
static_assert(offsetof(StereoManager, stack_top) == 0x6C);
static_assert(offsetof(StereoManager, reference_pose) == 0x78);
static_assert(offsetof(StereoManager, fade) == 0xAC);
static_assert(offsetof(StereoManager, si) == 0xB0);

// As load_state / store_state, for the attached device.
inline StereoInterface* load_device(StereoManager& m) noexcept {
    return std::atomic_ref(m.device).load(std::memory_order_relaxed);
}
inline void store_device(StereoManager& m, StereoInterface* device) noexcept {
    std::atomic_ref(m.device).store(device, std::memory_order_relaxed);
}

// The single instance the game is given.
StereoManager& manager() noexcept;

// True once the stereo device gave up on its VR runtime (init or session
// failure). The player has been told and VR stays off for this run.
bool runtime_failed() noexcept;

// Builds both vtables (four device slots point into AI.exe) and the
// initial field values. Call once from the hook installer.
// From: FUN_10003510 + FUN_10003640.
void initialize() noexcept;

// Replacements for three game stereo-manager calls (all __thiscall on the
// manager, installed as call-site redirects by the hook installer).
//
// Manager::CreateDevice(int enable): bring up the HMD backend and attach
// the device. Only the first call starts the runtime; later ones (a display
// reset) re-attach what it produced. From: FUN_1001f6a0 (ret 4).
void __fastcall create_device(StereoManager* self, void* edx, int enable) noexcept;
// Manager::GetRenderTargetSize(int* w, int* h): record the desktop window
// size, then size the stereo target. From: FUN_100202a0 (ret 8).
int* __fastcall get_render_target_size(StereoManager* self, void* edx, int* w, int* h) noexcept;
// Manager::GetHmdResolution(int* w, int* h): leave the window size alone.
// From: FUN_1001f640 (ret 8).
int* __fastcall get_hmd_resolution(StereoManager* self, void* edx, int* w, int* h) noexcept;

} // namespace mothervr::stereo
