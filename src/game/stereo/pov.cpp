// The POV accessors and the per-eye reflection pass (07).
// Analysis: dumps/07_reflections_head_pose.md.

#include "game/stereo/pov.h"

#include "game/clear_gate.h"
#include "game/code_address.h"
#include "game/stereo/field.h"
#include "util/math/matrix.h"

#include <array>
#include <cmath>
#include <cstring>

namespace omvr::game::stereo {

namespace {

using core::stereo::Matrix44;

using RenderFn  = void (__fastcall*)(void* pov, void* edx);
using ApplyFn   = void (__fastcall*)(void* manager, void* edx, std::int32_t eye, float* view, float* proj);
using FrustumFn = void (__fastcall*)(void* pov, void* edx);

// Bytes of one 4x4 float matrix.
constexpr std::size_t kMatrixBytes = sizeof(Matrix44);
// The centre state a pass changes and the end restores: view, projection
// and the projection copy. Each pass starts from the view and projection.
constexpr std::size_t kCentreBytes    = 3 * kMatrixBytes;
constexpr std::size_t kPassInputBytes = 2 * kMatrixBytes;
// The eyes of a side-by-side target.
constexpr std::int32_t kEyes = 2;

// The renderer's eye owner, null when the game has none.
void* eye_owner(const ReflectionTargets& targets) noexcept { return *targets.renderer_eye_owner; }

// Eye `eye`'s transform on the POV's centre view and projection, and the
// derived fields: what POV::SetEye does, without its lighting and
// reflection-plane side effects (07 §4.2).
void setup_eye(const ReflectionTargets& targets, StereoManager& manager, std::uint8_t* pov,
               std::int32_t eye) noexcept {
    const ApplyFn   apply   = CodeAddress{targets.apply_eye_transform};
    const FrustumFn frustum = CodeAddress{targets.update_frustum};
    field<std::int32_t>(pov, kPovEye) = eye;
    apply(&manager, nullptr, eye, &field<float>(pov, kPovView), &field<float>(pov, kPovProj));
    frustum(pov, nullptr);
    const float* proj    = &field<float>(pov, kPovProj);
    const float  x_scale = util::element4(proj, 0, 0);
    const float  y_scale = util::element4(proj, 1, 1);
    field<float>(pov, kPovAspect) = y_scale / x_scale;
    field<float>(pov, kPovFov)    = 2.0f * std::atan(1.0f / y_scale);
    std::memcpy(pov + kPovProjCopy, proj, kMatrixBytes);
    if (void* owner = eye_owner(targets)) field<std::int32_t>(owner, kRendererCurrentEye) = eye;
}

// What a per-eye render changes on the POV and the renderer, to put back.
struct CentreState {
    std::array<std::uint8_t, kCentreBytes> matrices;
    float        fov;
    float        aspect;
    std::int32_t eye;
    void*        owner;
    std::int32_t renderer_eye;
};

CentreState save_centre(const ReflectionTargets& targets, std::uint8_t* pov) noexcept {
    CentreState s{};
    std::memcpy(s.matrices.data(), pov, s.matrices.size());
    s.fov          = field<float>(pov, kPovFov);
    s.aspect       = field<float>(pov, kPovAspect);
    s.eye          = field<std::int32_t>(pov, kPovEye);
    s.owner        = eye_owner(targets);
    s.renderer_eye = s.owner ? field<std::int32_t>(s.owner, kRendererCurrentEye) : 0;
    return s;
}

void restore_centre(std::uint8_t* pov, const CentreState& s) noexcept {
    std::memcpy(pov, s.matrices.data(), s.matrices.size());
    field<float>(pov, kPovFov)        = s.fov;
    field<float>(pov, kPovAspect)     = s.aspect;
    field<std::int32_t>(pov, kPovEye) = s.eye;
    if (s.owner) field<std::int32_t>(s.owner, kRendererCurrentEye) = s.renderer_eye;
}

} // namespace

std::optional<Matrix44> view_matrix(const AddressMap& addresses) noexcept {
    void* manager = pointer_at(addresses.at(AiAddr::PovManagerPtr));
    const float* view = manager ? field_ptr<float>(manager, kPovManagerMainView) : nullptr;
    if (!view) return std::nullopt;
    Matrix44 m;
    std::memcpy(m.data(), view, kMatrixBytes);
    return m;
}

bool gate_reflection_clear(const ReflectionTargets& targets, core::diag::ports::Log& log) noexcept {
    return install_clear_gate(targets.clear, log);
}

std::uint8_t pov_flags(const void* pov) noexcept { return field<std::uint8_t>(pov, kPovFlags); }

bool pov_renders(const void* pov) noexcept {
    constexpr auto kActiveAndRenders = static_cast<std::uint8_t>(kPovFlagActive | kPovFlagRenders);
    return (pov_flags(pov) & kActiveAndRenders) == kActiveAndRenders;
}

std::int32_t pov_target_width(const void* pov) noexcept { return field<std::int32_t>(pov, kPovTargetWidth); }

std::int32_t pov_target_height(const void* pov) noexcept { return field<std::int32_t>(pov, kPovTargetHeight); }

void render_reflection_flat(const ReflectionTargets& targets, void* pov) noexcept {
    const RenderFn render = CodeAddress{targets.render};
    render(pov, nullptr);
}

void render_reflection_per_eye(const ReflectionTargets& targets, StereoManager& manager, void* pov,
                               AfterEyePass after_eye) noexcept {
    auto* p = static_cast<std::uint8_t*>(pov);
    const CentreState centre = save_centre(targets, p);
    for (std::int32_t eye = 0; eye < kEyes; ++eye) {
        std::memcpy(p, centre.matrices.data(), kPassInputBytes);
        setup_eye(targets, manager, p, eye);
        set_skip_clears(eye == 1);  // keep the first half (07 §4.3)
        render_reflection_flat(targets, pov);
        set_skip_clears(false);
        if (after_eye) after_eye(eye);
    }
    restore_centre(p, centre);
}

} // namespace omvr::game::stereo
