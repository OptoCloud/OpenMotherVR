// OpenMotherVR-Provenance: clean
// The POV accessors and the per-eye reflection pass (07).
// Analysis: dumps/07_reflections_head_pose.md.

#include "game/stereo/pov.h"

#include "game/clear_gate.h"
#include "game/code_address.h"
#include "game/detour.h"
#include "game/field.h"
#include "game/patcher.h"
#include "util/math/matrix.h"

#include <array>
#include <cmath>
#include <cstring>

namespace omvr::game::stereo {

namespace {

using core::stereo::Matrix44;

using RenderFn  = void(__fastcall*)(void* pov, void* edx);
using UpdateFn  = void(__fastcall*)(void* pov, void* edx);
using ApplyFn   = void(__fastcall*)(void* manager, void* edx, std::int32_t eye, float* view, float* proj);
using FrustumFn = void(__fastcall*)(void* pov, void* edx);

// Bytes of one 4x4 float matrix.
constexpr std::size_t kMatrixBytes = sizeof(Matrix44);
// The centre state a pass changes and the end restores: view, projection
// and the projection copy. Each pass starts from the view and projection.
constexpr std::size_t kCentreBytes    = 3 * kMatrixBytes;
constexpr std::size_t kPassInputBytes = 2 * kMatrixBytes;
// The eyes of a side-by-side target.
constexpr std::int32_t kEyes = 2;
// 07 §4.7: eye -1, the headset's shared field of view without an eye
// offset (02 §5.1, 03 §3.2).
constexpr std::int32_t kCentreEye = -1;

// The renderer's eye owner, null when the game has none.
void* eye_owner(const ReflectionTargets& targets) noexcept { return *targets.renderer_eye_owner; }

// What POV::SetEye derives from the POV's view and projection (07 §2.4,
// §4.2): the frustum, the aspect, the vertical FOV (the game's own
// formulas) and the projection copy.
void derive_from_projection(const ReflectionTargets& targets, std::uint8_t* pov) noexcept {
    const FrustumFn frustum = CodeAddress{targets.update_frustum};
    frustum(pov, nullptr);
    const float* proj = &field<float>(pov, kPovProj);
    const util::Matrix44View m{proj, util::kMatrix44Floats};
    const float x_scale           = util::element4(m, 0, 0);
    const float y_scale           = util::element4(m, 1, 1);
    field<float>(pov, kPovAspect) = y_scale / x_scale;
    field<float>(pov, kPovFov)    = 2.0f * std::atan(1.0f / y_scale);
    std::memcpy(pov + kPovProjCopy, proj, kMatrixBytes);
}

// Eye `eye`'s transform on the POV's centre view and projection, and the
// derived fields: what POV::SetEye does, without its lighting and
// reflection-plane side effects (07 §4.2).
void setup_eye(const ReflectionTargets& targets, StereoManager& manager, std::uint8_t* pov,
               std::int32_t eye) noexcept {
    const ApplyFn apply               = CodeAddress{targets.apply_eye_transform};
    field<std::int32_t>(pov, kPovEye) = eye;
    apply(&manager, nullptr, eye, &field<float>(pov, kPovView), &field<float>(pov, kPovProj));
    derive_from_projection(targets, pov);
    if (void* owner = eye_owner(targets)) field<std::int32_t>(owner, kRendererCurrentEye) = eye;
}

// What a per-eye render changes on the POV and the renderer, to put back.
struct CentreState {
    std::array<std::uint8_t, kCentreBytes> matrices;
    float fov;
    float aspect;
    std::int32_t eye;
    void* owner;
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
    void* manager     = pointer_at(addresses.at(AiAddr::PovManagerPtr));
    const float* view = manager ? field_ptr<float>(manager, kPovManagerMainView) : nullptr;
    if (!view) return std::nullopt;
    Matrix44 m;
    std::memcpy(m.data(), view, kMatrixBytes);
    return m;
}

bool gate_reflection_clear(const ReflectionTargets& targets, core::diag::ports::Log& log) noexcept {
    return install_clear_gate(targets.clear, log);
}

bool detour_reflection_halves(ReflectionTargets& targets, const ReflectionReplacements& replacements,
                              core::diag::ports::Log& log) noexcept {
    return detour(targets.viewport_setter, replacements.set_viewport, kViewportSetterPrologue,
                  targets.set_viewport_original, "viewport setter", log) &&
           detour(targets.vs_constant_writer, replacements.write_vs_constants, kVsConstantWriterPrologue,
                  targets.write_vs_constants_original, "vertex constant writer", log);
}

bool redirect_reflection_update(const ReflectionTargets& targets, const ReflectionReplacements& replacements,
                                core::diag::ports::Log& log) noexcept {
    if (!targets.update_call ||
        !calls(targets.update_call, static_cast<const std::uint8_t*>(targets.update))) {
        return false;
    }
    Patcher patcher{log};
    return patcher.redirect_call(targets.update_call, replacements.update, "reflection update").has_value();
}

std::uint8_t pov_flags(const void* pov) noexcept { return field<std::uint8_t>(pov, kPovFlags); }

bool pov_active(const void* pov) noexcept { return (pov_flags(pov) & kPovFlagActive) != 0; }

bool pov_renders(const void* pov) noexcept {
    constexpr auto kActiveAndRenders = static_cast<std::uint8_t>(kPovFlagActive | kPovFlagRenders);
    return (pov_flags(pov) & kActiveAndRenders) == kActiveAndRenders;
}

std::int32_t reflection_quality(const ReflectionTargets& targets) noexcept {
    const void* manager = *targets.reflection_manager;
    return manager ? field<std::int32_t>(manager, kReflectionQuality) : -1;
}

std::int32_t pov_target_width(const void* pov) noexcept { return field<std::int32_t>(pov, kPovTargetWidth); }

std::int32_t pov_target_height(const void* pov) noexcept {
    return field<std::int32_t>(pov, kPovTargetHeight);
}

void render_reflection_flat(const ReflectionTargets& targets, void* pov) noexcept {
    const RenderFn render = CodeAddress{targets.render};
    render(pov, nullptr);
}

void render_reflection_per_eye(const ReflectionTargets& targets, StereoManager& manager, void* pov,
                               const EyePassCallbacks& callbacks) noexcept {
    auto* p                  = static_cast<std::uint8_t*>(pov);
    const CentreState centre = save_centre(targets, p);
    for (std::int32_t eye = 0; eye < kEyes; ++eye) {
        std::memcpy(p, centre.matrices.data(), kPassInputBytes);
        setup_eye(targets, manager, p, eye);
        set_skip_clears(eye == 1); // keep the first half (07 §4.3)
        if (callbacks.before) callbacks.before(eye);
        render_reflection_flat(targets, pov);
        set_skip_clears(false);
        if (callbacks.after) callbacks.after(eye);
    }
    restore_centre(p, centre);
}

void update_reflection(const ReflectionTargets& targets, void* pov) noexcept {
    const UpdateFn update = CodeAddress{targets.update};
    update(pov, nullptr);
}

void widen_reflection_projection(const ReflectionTargets& targets, StereoManager& manager,
                                 void* pov) noexcept {
    // ApplyEyeTransform on copies (07 §4.7): only the projection is taken,
    // the view stays the update's.
    auto* p             = static_cast<std::uint8_t*>(pov);
    const ApplyFn apply = CodeAddress{targets.apply_eye_transform};
    Matrix44 view;
    Matrix44 proj;
    std::memcpy(view.data(), p + kPovView, kMatrixBytes);
    std::memcpy(proj.data(), p + kPovProj, kMatrixBytes);
    apply(&manager, nullptr, kCentreEye, view.data(), proj.data());
    std::memcpy(p + kPovProj, proj.data(), kMatrixBytes);
    derive_from_projection(targets, p);
}

void* viewport_d3d_context(void* context) noexcept { return field<void*>(context, kViewportContextD3d); }

const float* screen_mapping_in(std::int32_t buffer, std::int32_t first_register, const float* data,
                               std::int32_t register_count) noexcept {
    if (buffer != kScreenMappingBuffer || !data) return nullptr;
    const std::int64_t index = std::int64_t{kScreenMappingRegister} - first_register;
    if (index < 0 || index >= register_count) return nullptr;
    return data + (static_cast<std::size_t>(index) * kFloat4Floats);
}

void write_screen_mapping(const ReflectionTargets& targets, void* renderer, const float* mapping) noexcept {
    targets.write_vs_constants_original(renderer, kScreenMappingBuffer, kScreenMappingRegister, mapping, 1);
}

} // namespace omvr::game::stereo
