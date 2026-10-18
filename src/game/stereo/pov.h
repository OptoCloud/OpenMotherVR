// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/stereo/types.h"
#include "game/addresses.h"
#include "game/layout.h"
#include "game/x86.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

// The game's points of view (POV): the main view and the reflection view
// (07 §1, §2).
namespace omvr::game::stereo {

// POV fields (07 §1).
inline constexpr std::ptrdiff_t kPovView         = 0x000; // float[16]
inline constexpr std::ptrdiff_t kPovProj         = 0x040; // float[16]
inline constexpr std::ptrdiff_t kPovProjCopy     = 0x080; // float[16], used for shader constants
inline constexpr std::ptrdiff_t kPovFov          = 0x0E0; // vertical FOV, radians
inline constexpr std::ptrdiff_t kPovAspect       = 0x0E4;
inline constexpr std::ptrdiff_t kPovTargetWidth  = 0x33C; // its render target's size
inline constexpr std::ptrdiff_t kPovTargetHeight = 0x340;
inline constexpr std::ptrdiff_t kPovFlags        = 0x36B; // see the flag bits below
inline constexpr std::ptrdiff_t kPovEye          = 0x378; // -1 none; selects the viewport half

// The reflection manager's quality, setting 0x10's value (07 §1).
inline constexpr std::ptrdiff_t kReflectionQuality = 0x60;

// POV flag bits (+0x36B).
inline constexpr std::uint8_t kPovFlagActive  = 0x01;
inline constexpr std::uint8_t kPovFlagRenders = 0x10;

// The POV manager holds a pointer to the main view's matrix here.
inline constexpr std::ptrdiff_t kPovManagerMainView = 0x08;
// The renderer's current eye, an int in the eye owner (07 §1).
inline constexpr std::ptrdiff_t kRendererCurrentEye = 0x2C;

// The viewport setter's argument (07 §4.8): {x, y, width, height} in
// pixels, then the depth range.
struct GameViewport {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
    float min_depth;
    float max_depth;
};
static_assert(sizeof(GameViewport) == 24 && offsetof(GameViewport, min_depth) == 16);

// The renderer's viewport setter and vertex constant writer (07 §4.8),
// both __cdecl.
using SetViewportFn      = void(__cdecl*)(void* context, const GameViewport* viewport);
using WriteVsConstantsFn = void(__cdecl*)(void* renderer, std::int32_t buffer, std::int32_t first_register,
                                          const float* data, std::int32_t register_count);

// The setter's context holds its D3D11 device context here (07 §4.8).
inline constexpr std::ptrdiff_t kViewportContextD3d = 0x08;

// The screen-mapping constant: vertex buffer 1, register 21, one float4
// (07 §2.7).
inline constexpr std::int32_t kScreenMappingBuffer   = 1;
inline constexpr std::int32_t kScreenMappingRegister = 21;
inline constexpr std::size_t kFloat4Floats           = 4;

// 07 §4.8: the setter's first bytes, `push ebp; mov ebp, esp; sub esp,
// 0x18; mov eax, [ebp+0xC]`, and the writer's, `push ebp; mov ebp, esp;
// push esi; mov esi, [ebp+8]; push edi`.
inline constexpr std::uint8_t kViewportFrameSize = 0x18;
inline constexpr std::uint8_t kSecondArgDisp     = 0x0C; // [ebp+0xC]
inline constexpr std::uint8_t kFirstArgDisp      = 0x08; // [ebp+8]
inline constexpr std::uint8_t kModRmEaxEbpDisp8 =
    x86::modrm(x86::kModDisp8, std::to_underlying(x86::Reg::Eax), std::to_underlying(x86::Reg::Ebp));
inline constexpr std::uint8_t kModRmEsiEbpDisp8 =
    x86::modrm(x86::kModDisp8, std::to_underlying(x86::Reg::Esi), std::to_underlying(x86::Reg::Ebp));
inline constexpr std::uint8_t kPushEdi = x86::kPushEax + std::to_underlying(x86::Reg::Edi);

inline constexpr auto kViewportSetterPrologue = x86::concat(
    x86::concat(x86::kFramePrologue,
                std::array<std::uint8_t, 3>{x86::kGroup1Imm8Signed, x86::kModRmSubEsp, kViewportFrameSize}),
    std::array<std::uint8_t, 3>{x86::kMovR32Rm32, kModRmEaxEbpDisp8, kSecondArgDisp});
static_assert(kViewportSetterPrologue ==
              std::array<std::uint8_t, 9>{0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18, 0x8B, 0x45, 0x0C});

inline constexpr auto kVsConstantWriterPrologue = x86::concat(
    x86::kFramePrologue,
    std::array<std::uint8_t, 5>{x86::kPushEsi, x86::kMovR32Rm32, kModRmEsiEbpDisp8, kFirstArgDisp, kPushEdi});
static_assert(kVsConstantWriterPrologue ==
              std::array<std::uint8_t, 8>{0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75, 0x08, 0x57});

// The AI.exe functions and data the per-eye reflections use, found by the
// installer (07 §5).
struct ReflectionTargets {
    void* render;              // ReflectionPOV render
    void* apply_eye_transform; // Stereo::ApplyEyeTransform
    void* update_frustum;      // POV frustum update
    void** renderer_eye_owner; // its +0x2C is the renderer's current eye
    std::uint8_t* clear;       // the game's clear of the bound target
    void** reflection_manager; // its +0x60 is the planar reflection quality
    void* update;              // ReflectionPOV update (07 §2.1)
    // Its call (07 §4.7); null when that call is not the expected one.
    std::uint8_t* update_call;
    std::uint8_t* viewport_setter;    // 07 §4.8
    std::uint8_t* vs_constant_writer; // 07 §4.8
    // The ways back into the setter and the writer, set before their
    // detours go live.
    SetViewportFn set_viewport_original            = nullptr;
    WriteVsConstantsFn write_vs_constants_original = nullptr;
};

// What the installer put in for the per-eye reflections (07 §5).
struct ReflectionInstall {
    bool clear_gated      = false; // the clear gate (07 §4.3)
    bool halves           = false; // the viewport and screen-mapping detours (07 §4.8)
    bool covers_both_eyes = false; // the update redirect (07 §4.7)
};

// The loader's replacements for the setter, the writer and the call of the
// update (07 §4.7, §4.8).
struct ReflectionReplacements {
    SetViewportFn set_viewport;
    WriteVsConstantsFn write_vs_constants;
    const void* update; // __thiscall, POV in ECX, no args, plain ret
};

// The main view's world-to-view matrix (row vectors, left-handed, metres):
// columns 0..2 are the camera's right, up and forward axes in world space.
// Nullopt before the game has a POV manager. Game thread.
std::optional<core::stereo::Matrix44> view_matrix(const AddressMap& addresses) noexcept;

// Gates the game's clear (game/clear_gate) of `targets`. False if the clear
// is not the expected code; the fix then stays off. Once, at attach.
bool gate_reflection_clear(const ReflectionTargets& targets, core::diag::ports::Log& log) noexcept;

// Detours the viewport setter and the vertex constant writer to
// `replacements` (07 §4.8), their trampolines into `targets`. False if
// either is not the expected code; the fix then stays off. Once, at attach.
bool detour_reflection_halves(ReflectionTargets& targets, const ReflectionReplacements& replacements,
                              core::diag::ports::Log& log) noexcept;

// Points the call of the reflection update at `replacements.update` (07
// §4.7). False without a verified call; the fix then runs without it.
bool redirect_reflection_update(const ReflectionTargets& targets, const ReflectionReplacements& replacements,
                                core::diag::ports::Log& log) noexcept;

// The flags of the POV `pov`.
std::uint8_t pov_flags(const void* pov) noexcept;
// The POV is active: the reflection manager has a plane (07 §2.1).
bool pov_active(const void* pov) noexcept;
// The POV is active and renders this frame.
bool pov_renders(const void* pov) noexcept;
// The planar reflection quality, setting 0x10's value (07 §1, 24 §1.9);
// -1 before the game has a reflection manager. Render thread.
std::int32_t reflection_quality(const ReflectionTargets& targets) noexcept;

// Its render target's size.
std::int32_t pov_target_width(const void* pov) noexcept;
std::int32_t pov_target_height(const void* pov) noexcept;

// The game's ReflectionPOV render on `pov`, as the game calls it (07 §2.2).
// Render thread. The functions below call the game through `targets`, set
// once at attach before the render call is redirected.
void render_reflection_flat(const ReflectionTargets& targets, void* pov) noexcept;

// Called with the eye right before each eye's pass and right after it,
// while the pass's target is still bound.
struct EyePassCallbacks {
    void (*before)(int eye) noexcept;
    void (*after)(int eye) noexcept;
};

// Renders the reflection POV `pov` once per eye into the two halves of its
// target (07 §4): each pass from the centre view with eye k's transform, the
// second pass with the game's clears skipped so the first half stays; the
// centre state is restored afterwards (07 §4.5). `manager` is the stereo
// manager the game applies the eye transform with. Render thread.
void render_reflection_per_eye(const ReflectionTargets& targets, StereoManager& manager, void* pov,
                               const EyePassCallbacks& callbacks) noexcept;

// The game's ReflectionPOV update on `pov` (07 §2.1). Game thread.
void update_reflection(const ReflectionTargets& targets, void* pov) noexcept;

// 07 §4.7: the projection of `pov` (+0x40 and its copy +0x80) replaced by
// the headset's eye -1 projection, and its frustum, aspect and FOV
// recomputed from it; the view is kept. Right after the update. Game
// thread.
void widen_reflection_projection(const ReflectionTargets& targets, StereoManager& manager,
                                 void* pov) noexcept;

// The setter's own D3D11 device context (an ID3D11DeviceContext*).
void* viewport_d3d_context(void* context) noexcept;

// The screen-mapping constant (07 §2.7) among `register_count` registers of
// buffer `buffer` from `first_register`: its four floats, or null when the
// write does not reach it.
const float* screen_mapping_in(std::int32_t buffer, std::int32_t first_register, const float* data,
                               std::int32_t register_count) noexcept;

// Writes the four floats `mapping` to the screen-mapping constant through
// the writer's trampoline. Render thread, during a pass.
void write_screen_mapping(const ReflectionTargets& targets, void* renderer, const float* mapping) noexcept;

} // namespace omvr::game::stereo
