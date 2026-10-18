// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/render/shader_fix_stats.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// Constant smoke and fog lighting (24 §15, Graphics.ConstantSmokeLighting).
// The game's lit particles read their light from the alpha-light atlas
// (`alphalightIntensitiesReg_TEX`, slot t9) with one sample of its x, y and
// z into three components of a temporary:
//
//   sample_indexable(texture2d) rN.xyz, coord, t9.xyzw, sM
//   sample_indexable(texture2d) rN.yzw, coord, t9.wxyz, sM
//
// The fix replaces that sample with `mov rN.<same mask>, l(B, B, B, B)`, so
// the three components hold the constant brightness B and the temporary's
// other component (in the yzw form, the soft depth fade computed before the
// sample) is kept. Everything else of the shader stays. Pure; runs on
// whichever thread creates the shader.
namespace omvr::core::render {

// The pixel shader `blob` with its atlas sample replaced by (B, B, B), B =
// `brightness`, or an empty vector when it does not bind the atlas at t9
// by its name, samples it other than once in one of the two forms, or
// anything about the container is unexpected (then the original is used).
std::vector<std::uint8_t> patch_constant_smoke(const void* blob, std::size_t size, float brightness) noexcept;

// The keys as they were when the game created its device (24 §15.3: a
// change applies at the next launch), and the counts of the shaders the fix
// changed (24 §15.6). Set once on the thread that creates the device, read
// on any thread that creates shaders.
class ConstantSmoke final {
public:
    explicit ConstantSmoke(diag::ports::Log& log) noexcept
        : log_(log), stats_(log, ShaderFix::ConstantSmoke) {}

    // Turns the fix on with `brightness` and logs it, or leaves it off.
    void start(bool on, float brightness) noexcept;
    // The brightness while the fix is on.
    std::optional<float> brightness() const noexcept;
    ShaderFixStats& stats() noexcept { return stats_; }

private:
    diag::ports::Log& log_;
    ShaderFixStats stats_;
    std::atomic<bool> on_{false};
    std::atomic<float> brightness_{0.0f};
};

} // namespace omvr::core::render
