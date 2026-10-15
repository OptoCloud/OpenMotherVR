#include "core/stereo/stereo_aa.h"

#include "core/config/settings_store.h"
#include "core/config/settings_table.h"
#include "core/diag/logger.h"
#include "core/stereo/stereo_device.h"
#include "game/stereo/follow_camera.h"  // R: stereo (A3): the PostFX reads in the thunk
#include "game/stereo/postfx.h"         // R: stereo (A3): the PostFX calls in the thunk

#include <utility>

namespace omvr::core::stereo {

using namespace game::stereo;

void StereoAa::log_stereo_aa(std::int32_t mode, bool run) noexcept {
    const config::StereoAA setting = config::current().rendering.stereo_aa;
    const int action = !run ? 0 : mode == kSmaaT2x ? 2 : 1;
    if (mode == s_mode && action == s_action) return;
    s_mode   = mode;
    s_action = action;
    constexpr const char* kModes[]   = {"off", "FXAA", "SMAA T1x", "SMAA T2x"};
    constexpr const char* kActions[] = {"skipped", "runs it", "runs it as SMAA T1x"};
    const char* name = mode >= kAaOff && mode <= kSmaaT2x ? kModes[mode] : "unknown";
    diag::logf("[stereo] Stereo anti-aliasing for game mode {}: {} (setting {})", name,
               mode == kAaOff ? "nothing to run" : kActions[action], config::to_string(setting));
}

void StereoAa::final_draw() noexcept {
    void* postfx = stereo_.running() ? postfx_instance() : nullptr;
    const std::int32_t mode = postfx ? field<std::int32_t>(postfx, kPostFxAaMode) : kAaOff;
    // StereoAntiAliasing: off, or Auto above the pixel scale where the eyes
    // are already supersampled (about 0.6 ms GPU for little gain).
    const bool run = mode >= kFxaa && mode <= kSmaaT2x && config::stereo_aa_enabled(config::current().rendering);
    if (postfx) log_stereo_aa(mode, run);
    if (!run) {
        call_fullscreen_draw();   // as the game does
        return;
    }
    // The AA pass draws the bound effect (postprocess_stereo_final, fade
    // already in c36) into its intermediate, then filters into the output.
    // The stereo path always draws with the full-screen triangle; keep that.
    auto& mesh_draw = field<std::uint8_t>(postfx, kPostFxMeshDraw);
    const std::uint8_t saved_mesh_draw = std::exchange(mesh_draw, std::uint8_t{0});
    call_aa_pass(postfx);
    mesh_draw = saved_mesh_draw;
}

void StereoAa::run_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept {
    // Spatial SMAA in stereo; it also zeroes the projection jitter that
    // the temporal mode would apply next frame.
    const std::uint8_t spatial = 0;
    if (*temporal && stereo_.running()) temporal = &spatial;
    call_smaa(color, second, temporal);
}

} // namespace omvr::core::stereo
