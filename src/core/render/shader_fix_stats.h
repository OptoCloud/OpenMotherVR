// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

#include <atomic>

namespace omvr::core::render {

// Which fix a ShaderFixStats counts: the dither fix of the pixel shaders
// (08 §1) or the upright-sprite fix of the vertex shaders (24 §14).
enum class ShaderFix { Dither, UprightSprites };

// Counts the shaders of one stage the game creates and the ones a fix
// rewrote (08 §1, 24 §14), and logs them at a low rate. Any thread that
// creates shaders, so the counters are atomic (sequentially consistent: a
// few increments per shader the game creates, never per frame).
class ShaderFixStats final {
public:
    explicit ShaderFixStats(diag::ports::Log& log, ShaderFix fix = ShaderFix::Dither) noexcept
        : log_(log), fix_(fix) {}

    // One more shader created; returns the total.
    int count_created() noexcept { return ++created_; }
    // The fixed shader of creation `total` was accepted.
    void count_fixed(int total) noexcept;
    // The shader of creation `total` holds the dither's constants but was
    // left unchanged (11 §2.2 r8: a site in a form no layout of 08 §1.1
    // matches). Dither fix only.
    void count_unchanged(int total) noexcept;
    // The driver refused a fixed shader with `hr`; the original is used.
    void count_rejected(long hr) noexcept;

private:
    void log_counts(int total, int fixed) noexcept;

    diag::ports::Log& log_;
    ShaderFix fix_;
    std::atomic<int> created_{0};
    std::atomic<int> fixed_{0};
    std::atomic<int> unchanged_{0};
    std::atomic<int> rejected_{0};
};

} // namespace omvr::core::render
