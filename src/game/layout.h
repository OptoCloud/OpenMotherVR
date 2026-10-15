#pragma once

#include "game/constants.h"

#include <array>
#include <cstddef>
#include <cstdint>

struct ID3D11Texture2D;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11Buffer;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;

namespace omvr::core::vr::ports {
class VrRuntime;
} // namespace omvr::core::vr::ports

// Structures of AI.exe the mod reads or writes, with the game's layout.
namespace omvr::game {

// ---- LibOVR 0.4 types the game's stereo path uses -----------------------------

struct Sizei {
    std::int32_t w, h;
};
static_assert(sizeof(Sizei) == 0x8);
static_assert(offsetof(Sizei, w) == 0x0);
static_assert(offsetof(Sizei, h) == 0x4);

// Tangents of the half-angles, LibOVR ovrFovPort layout.
struct FovPort {
    float up, down, left, right;
};
static_assert(sizeof(FovPort) == 0x10);
static_assert(offsetof(FovPort, up) == 0x0);
static_assert(offsetof(FovPort, down) == 0x4);
static_assert(offsetof(FovPort, left) == 0x8);
static_assert(offsetof(FovPort, right) == 0xC);

// LibOVR 0.4.0 ovrHmdDesc. The game's stereo device embeds one at +0x10.
struct HmdDesc {
    void* handle;                     // +0x00
    std::int32_t type;                // +0x04
    const char* product_name;         // +0x08  out
    const char* manufacturer;         // +0x0C  out
    std::uint32_t hmd_caps;           // +0x10
    std::uint32_t sensor_caps;        // +0x14
    std::uint32_t distortion_caps;    // +0x18
    Sizei resolution;                 // +0x1C
    std::int32_t windows_pos[2];      // +0x24
    FovPort default_eye_fov[2];       // +0x2C  out: each eye's real FOV
    FovPort max_eye_fov[2];           // +0x4C  out: the symmetric FOV the game renders
    std::int32_t eye_render_order[2]; // +0x6C
    const char* display_device_name;  // +0x74
    std::int32_t display_id;          // +0x78
};
static_assert(sizeof(HmdDesc) == 0x7C);
static_assert(offsetof(HmdDesc, handle) == 0x00);
static_assert(offsetof(HmdDesc, type) == 0x04);
static_assert(offsetof(HmdDesc, product_name) == 0x08);
static_assert(offsetof(HmdDesc, manufacturer) == 0x0C);
static_assert(offsetof(HmdDesc, hmd_caps) == 0x10);
static_assert(offsetof(HmdDesc, sensor_caps) == 0x14);
static_assert(offsetof(HmdDesc, distortion_caps) == 0x18);
static_assert(offsetof(HmdDesc, resolution) == 0x1C);
static_assert(offsetof(HmdDesc, windows_pos) == 0x24);
static_assert(offsetof(HmdDesc, default_eye_fov) == 0x2C);
static_assert(offsetof(HmdDesc, max_eye_fov) == 0x4C);
static_assert(offsetof(HmdDesc, eye_render_order) == 0x6C);
static_assert(offsetof(HmdDesc, display_device_name) == 0x74);
static_assert(offsetof(HmdDesc, display_id) == 0x78);

// ---- The game's stereo objects, replaced by ours --------------------------------
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

struct Recti {
    std::int32_t x, y, w, h;
};
static_assert(sizeof(Recti) == 0x10);
static_assert(offsetof(Recti, x) == 0x0);
static_assert(offsetof(Recti, y) == 0x4);
static_assert(offsetof(Recti, w) == 0x8);
static_assert(offsetof(Recti, h) == 0xC);

// LibOVR 0.4.0 ovrEyeRenderDesc. Only view_adjust is used.
struct EyeRenderDesc {
    std::int32_t eye;
    FovPort fov;
    Recti distorted_viewport;
    float pixels_per_tan[2];
    float view_adjust[3]; // eye offset from the head, scaled
};
static_assert(sizeof(EyeRenderDesc) == 0x38);
static_assert(offsetof(EyeRenderDesc, eye) == 0x00);
static_assert(offsetof(EyeRenderDesc, fov) == 0x04);
static_assert(offsetof(EyeRenderDesc, distorted_viewport) == 0x14);
static_assert(offsetof(EyeRenderDesc, pixels_per_tan) == 0x24);
static_assert(offsetof(EyeRenderDesc, view_adjust) == 0x2C);

// LibOVR 0.4.0 ovrD3D11Texture.
struct EyeTexture {
    std::int32_t api; // kEyeTextureApiD3d11
    Sizei size;
    Recti viewport;
    ID3D11Texture2D* texture;
    ID3D11ShaderResourceView* srv;
    std::uintptr_t reserved[6];
};
static_assert(sizeof(EyeTexture) == 0x3C);
static_assert(offsetof(EyeTexture, api) == 0x00);
static_assert(offsetof(EyeTexture, size) == 0x04);
static_assert(offsetof(EyeTexture, viewport) == 0x0C);
static_assert(offsetof(EyeTexture, texture) == 0x1C);
static_assert(offsetof(EyeTexture, srv) == 0x20);
static_assert(offsetof(EyeTexture, reserved) == 0x24);

struct Posef {
    float orientation[4];
    float position[3];
};
static_assert(sizeof(Posef) == 0x1C);
static_assert(offsetof(Posef, orientation) == 0x00);
static_assert(offsetof(Posef, position) == 0x10);

enum class DeviceState : std::int32_t {
    None    = 0, // no HMD
    Ready   = 1, // initialised
    Running = 2, // "sensor started": the game renders in stereo
};

// The stereo device. +0x000..+0x1F3 is the game's layout.
struct StereoInterface {
    void* const* vtable;       // +0x000
    DeviceState state;         // +0x004
    std::uint32_t frame_index; // +0x008
    void* hmd;                 // +0x00C  game: ovrHmd. Always null
    HmdDesc desc;              // +0x010
    Recti viewport[2];         // +0x08C  eye halves of the render target
    FovPort eye_fov[2];        // +0x0AC  the FOV the game renders
    EyeRenderDesc eye_desc[2]; // +0x0CC
    EyeTexture eye_texture[2]; // +0x13C
    Posef eye_pose[2];         // +0x1B4  unused
    std::int32_t rt_width;     // +0x1EC  both eyes side by side
    std::int32_t rt_height;    // +0x1F0
    // ---- MotherVR ----
    core::vr::ports::VrRuntime* backend; // +0x1F4
    std::int32_t runtime_id;             // +0x1F8  0 none, 1 Oculus, 2 OpenVR
    std::int32_t handle_seed;            // +0x1FC
    std::uint32_t unused_200;            // +0x200
    float world_scale;                   // +0x204  -vrscale
    std::int32_t pose_fetched;           // +0x208  head pose read since the last submit (atomic)
    std::int32_t mirror_width;           // +0x20C  desktop window size
    std::int32_t mirror_height;          // +0x210
    std::int32_t configured_width;       // +0x214  size of the eye texture in use
    std::int32_t configured_height;      // +0x218  (unused in the original)
    // The mirror (stereo_device.cpp, mirror.hlsl). One shader for every
    // mode; the frame kind picks the constant buffer. The original had a
    // second pixel shader at +0x224 for screen frames.
    ID3D11VertexShader* mirror_vs;      // +0x21C
    ID3D11PixelShader* mirror_ps;       // +0x220
    ID3D11Buffer* mirror_cb_screen;     // +0x224  screen frames
    ID3D11Buffer* mirror_cb;            // +0x228  head-tracked frames
    ID3D11SamplerState* mirror_sampler; // +0x22C
};
// The offsets of the game part; the mod's region (+0x1F4..) is asserted in
// stereo_extension.h.
static_assert(offsetof(StereoInterface, vtable) == 0x000);
static_assert(offsetof(StereoInterface, state) == 0x004);
static_assert(offsetof(StereoInterface, frame_index) == 0x008);
static_assert(offsetof(StereoInterface, hmd) == 0x00C);
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
static_assert(offsetof(StereoInterface, rt_height) == 0x1F0);

// The stereo manager. +0x00..+0xAF is the game's layout.
struct StereoManager {
    void* const* vtable;        // +0x000
    StereoInterface* device;    // +0x004  null when there is no HMD
    void* stereo_rt;            // +0x008  the game's stereo render target
    std::uint8_t game_0c[0x14]; // +0x00C
    float game_matrix[16];      // +0x020
    std::int32_t mode;          // +0x060
    float game_64;              // +0x064  scene visibility
    // The game's mode stack, a growable int array: it starts with no
    // storage and only the game's append routine allocates it.
    std::int32_t* stack_data;    // +0x068
    std::int32_t stack_top;      // +0x06C  count, kept equal to mode
    std::int32_t stack_capacity; // +0x070
    std::uint8_t game_74[4];     // +0x074
    float reference_pose[12];    // +0x078  recenter pose, 3x4
    std::uint8_t game_a8;        // +0x0A8
    std::uint8_t game_a9[3];
    float fade;         // +0x0AC
    StereoInterface si; // +0x0B0
};
static_assert(offsetof(StereoManager, vtable) == 0x000);
static_assert(offsetof(StereoManager, device) == 0x004);
static_assert(offsetof(StereoManager, stereo_rt) == 0x008);
static_assert(offsetof(StereoManager, game_0c) == 0x00C);
static_assert(offsetof(StereoManager, game_matrix) == 0x020);
static_assert(offsetof(StereoManager, mode) == 0x60);
static_assert(offsetof(StereoManager, game_64) == 0x64);
static_assert(offsetof(StereoManager, stack_data) == 0x68);
static_assert(offsetof(StereoManager, stack_top) == 0x6C);
static_assert(offsetof(StereoManager, stack_capacity) == 0x70);
static_assert(offsetof(StereoManager, game_74) == 0x74);
static_assert(offsetof(StereoManager, reference_pose) == 0x78);
static_assert(offsetof(StereoManager, game_a8) == 0xA8);
static_assert(offsetof(StereoManager, game_a9) == 0xA9);
static_assert(offsetof(StereoManager, fade) == 0xAC);
static_assert(offsetof(StereoManager, si) == 0xB0);
// The game's part is the size it allocates (02 §3.1).
static_assert(offsetof(StereoManager, si) == kGameStereoManagerSize);

// ---- The game's input manager (04 §2.1, §2.2, §2.4) ---------------------------
//
// By offset: the game has no symbols.

// The three action maps inside the manager.
inline constexpr std::array<std::ptrdiff_t, kActionMapCount> kInputManagerMapOffsets = {0x48, 0x2998, 0x3A0C};
// The devices the stereo update polls; `pad` is the active pad device.
inline constexpr std::ptrdiff_t kInputManagerMouse    = 0x4A70;
inline constexpr std::ptrdiff_t kInputManagerKeyboard = 0x4A74;
inline constexpr std::ptrdiff_t kInputManagerPad      = 0x4A7C;
// A device's "active device" byte.
inline constexpr std::ptrdiff_t kInputDeviceActive = 0x11;
// Indices of GameActionMap::device_by_type.
inline constexpr std::size_t kDeviceTypePad      = 0;
inline constexpr std::size_t kDeviceTypeKeyboard = 1;

// ---- The game's input action maps (by offset; the game has no symbols) --------

struct GameBinding {
    std::int16_t code;
    std::uint8_t device; // 0 pad, 1 keyboard, 2 mouse
    std::uint8_t flags;
    std::uint32_t unknown;
};
static_assert(sizeof(GameBinding) == 8);
static_assert(offsetof(GameBinding, code) == 0);
static_assert(offsetof(GameBinding, device) == 2);
static_assert(offsetof(GameBinding, flags) == 3);
static_assert(offsetof(GameBinding, unknown) == 4);

struct GameDigital {
    std::uint32_t unknown;
    GameBinding bindings[kMaxBindings];
    std::int32_t count;
    std::uint8_t rest[0x08];
};
static_assert(sizeof(GameDigital) == 0x40);
static_assert(offsetof(GameDigital, unknown) == 0x00);
static_assert(offsetof(GameDigital, bindings) == 0x04);
static_assert(offsetof(GameDigital, count) == 0x34);
static_assert(offsetof(GameDigital, rest) == 0x38);

struct GameAnalog {
    std::uint32_t unknown;
    GameBinding bindings[kMaxBindings];
    std::int32_t count;
    std::uint8_t rest[0x0C];
};
static_assert(sizeof(GameAnalog) == 0x44);
static_assert(offsetof(GameAnalog, unknown) == 0x00);
static_assert(offsetof(GameAnalog, bindings) == 0x04);
static_assert(offsetof(GameAnalog, count) == 0x34);
static_assert(offsetof(GameAnalog, rest) == 0x38);

// A binding list the remap blanks out (purpose unknown, see the analysis).
struct GameComposite {
    std::uint32_t codes[kCompositeCodes];
    std::int32_t count;
    std::uint8_t unknown;
    std::uint8_t keep;
    std::uint8_t pad[2];
};
static_assert(sizeof(GameComposite) == 0x24);
static_assert(offsetof(GameComposite, codes) == 0x00);
static_assert(offsetof(GameComposite, count) == 0x1C);
static_assert(offsetof(GameComposite, unknown) == 0x20);
static_assert(offsetof(GameComposite, keep) == 0x21);
static_assert(offsetof(GameComposite, pad) == 0x22);

struct GameActionMap {
    void* vftable;
    GameDigital digital[kMaxDigitalActions]; // +0x004
    GameAnalog analog[kMaxAnalogActions];    // +0xA44
    GameComposite composite[kMaxComposites]; // +0xE84 (capacity inferred)
    std::uint8_t unknown_1034[4];
    std::int32_t composite_count; // +0x1038
    void* device_by_type[3];      // +0x103C  pad, keyboard, mouse
    std::uint8_t unknown[0x0C];
    std::int32_t num_digital; // +0x1054
    std::int32_t num_analog;  // +0x1058
};
static_assert(sizeof(GameActionMap) == 0x105C);
static_assert(offsetof(GameActionMap, vftable) == 0x000);
static_assert(offsetof(GameActionMap, digital) == 0x004);
static_assert(offsetof(GameActionMap, analog) == 0xA44);
static_assert(offsetof(GameActionMap, composite) == 0xE84);
static_assert(offsetof(GameActionMap, unknown_1034) == 0x1034);
static_assert(offsetof(GameActionMap, composite_count) == 0x1038);
static_assert(offsetof(GameActionMap, device_by_type) == 0x103C);
static_assert(offsetof(GameActionMap, unknown) == 0x1048);
static_assert(offsetof(GameActionMap, num_digital) == 0x1054);
static_assert(offsetof(GameActionMap, num_analog) == 0x1058);

} // namespace omvr::game
