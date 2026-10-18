// OpenMotherVR-Provenance: clean
#include "loader/hooks/alpha_light_hooks.h"

#include "core/render/alpha_light.h"
#include "game/stereo/alpha_light.h"
#include "loader/context.h"

#include <array>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

using core::render::NdcScaleOffset;

// The game's Stereo::EyeNdc: thiscall, ret 8.
using EyeNdcFn = float*(__fastcall*)(void* manager, void* edx, float* out, std::int32_t eye);
static_assert(std::is_convertible_v<decltype(&alpha_light_eye_ndc), EyeNdcFn>);

// The eye the game uses for the centre camera (POV +0x378 = -1).
constexpr std::int32_t kCentreEye = -1;

// The four floats in the game's order, slot 14's (offset x, offset y,
// scale x, scale y).
static_assert(sizeof(NdcScaleOffset) == 4 * sizeof(float));
static_assert(std::is_trivially_copyable_v<NdcScaleOffset>);

NdcScaleOffset read(const float* v) noexcept {
    return {.offset_x = v[0], .offset_y = v[1], .scale_x = v[2], .scale_y = v[3]};
}

void write(float* out, const NdcScaleOffset& v) noexcept {
    out[0] = v.offset_x;
    out[1] = v.offset_y;
    out[2] = v.scale_x;
    out[3] = v.scale_y;
}

constexpr const char* kCorrectedLog =
    "[diag] particle lighting: eye {} lookup scale {:.3f} x {:.3f}, offset {:.3f} {:.3f}; used scale "
    "{:.3f} x {:.3f}, offset {:.3f} {:.3f}";

// 08 §8.5.
constexpr const char* kCameraGapLog =
    "[diag] Particle lighting: atlas camera vs eye {}: {:.1f} deg, {:.3f} m apart";

// Dev. Debug Options, every 900 frames per eye: how far the drawn eye is
// from the camera the atlas was laid out with (08 §8.5). Off where the
// cameras' addresses are unknown.
void trace_camera_gap(Context& ctx, std::int32_t eye) noexcept {
    const auto& targets = ctx.targets.render_fixes.alpha_light;
    if (!ctx.alpha_light_trace.camera_sample_due(eye)) return;
    const auto atlas = game::stereo::read_atlas_camera(targets);
    const auto view  = game::stereo::read_drawn_view(targets);
    if (!atlas || !view) return;
    const auto gap = core::render::atlas_camera_gap(atlas->forward, atlas->position, *view);
    core::diag::ports::logf(ctx.log, kCameraGapLog, eye, gap.angle_deg, gap.distance_m);
}

} // namespace

// In stereo the atlas was laid out with the centre camera (slot 14 for eye
// -1), so the lookup gets the eye's NDC relative to that (08 §8.3).
// Flat, or without a running device, the game's own answer.
float* __fastcall alpha_light_eye_ndc(void* manager, void*, float* out, std::int32_t eye) noexcept {
    auto& ctx           = context();
    const auto& targets = ctx.targets.render_fixes.alpha_light;
    float* const result = game::stereo::call_eye_ndc(targets, manager, out, eye);
    if (!ctx.stereo.running()) return result;
    std::array<float, 4> centre{};
    game::stereo::call_eye_ndc(targets, manager, centre.data(), kCentreEye);
    const NdcScaleOffset game = read(out);
    const NdcScaleOffset used = core::render::eye_ndc_in_centre(game, read(centre.data()));
    write(out, used);
    if (ctx.log.debug_enabled() && ctx.alpha_light_trace.changed(eye, game)) {
        core::diag::ports::logf(ctx.log, kCorrectedLog, eye, game.scale_x, game.scale_y, game.offset_x,
                                game.offset_y, used.scale_x, used.scale_y, used.offset_x, used.offset_y);
    }
    if (ctx.log.debug_enabled()) trace_camera_gap(ctx, eye);
    return out;
}

} // namespace omvr::loader::hooks
