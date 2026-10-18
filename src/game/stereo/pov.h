// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/stereo/types.h"
#include "game/addresses.h"
#include "game/layout.h"

#include <cstddef>
#include <cstdint>
#include <optional>

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

// The AI.exe functions and data the per-eye reflections use, found by the
// installer (07 §5).
struct ReflectionTargets {
    void* render;              // ReflectionPOV render
    void* apply_eye_transform; // Stereo::ApplyEyeTransform
    void* update_frustum;      // POV frustum update
    void** renderer_eye_owner; // its +0x2C is the renderer's current eye
    std::uint8_t* clear;       // the game's clear of the bound target
    void** reflection_manager; // its +0x60 is the planar reflection quality
};

// The main view's world-to-view matrix (row vectors, left-handed, metres):
// columns 0..2 are the camera's right, up and forward axes in world space.
// Nullopt before the game has a POV manager. Game thread.
std::optional<core::stereo::Matrix44> view_matrix(const AddressMap& addresses) noexcept;

// Gates the game's clear (game/clear_gate) of `targets`. False if the clear
// is not the expected code; the fix then stays off. Once, at attach.
bool gate_reflection_clear(const ReflectionTargets& targets, core::diag::ports::Log& log) noexcept;

// The flags of the POV `pov`.
std::uint8_t pov_flags(const void* pov) noexcept;
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

// Called after each eye's pass with the eye, while the pass's target is
// still bound.
using AfterEyePass = void (*)(int eye) noexcept;

// Renders the reflection POV `pov` once per eye into the two halves of its
// target (07 §4): each pass from the centre view with eye k's transform, the
// second pass with the game's clears skipped so the first half stays; the
// centre state is restored afterwards (07 §4.5). `manager` is the stereo
// manager the game applies the eye transform with. Render thread.
void render_reflection_per_eye(const ReflectionTargets& targets, StereoManager& manager, void* pov,
                               AfterEyePass after_eye) noexcept;

} // namespace omvr::game::stereo
