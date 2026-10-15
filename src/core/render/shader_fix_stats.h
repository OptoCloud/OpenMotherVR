// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

#include <atomic>

namespace omvr::core::render {

// Counts the pixel shaders the game creates and the ones the dither fix
// rewrote (08 §1), and logs them at a low rate. Any thread that creates
// shaders, so the counters are atomic (sequentially consistent: a few
// increments per shader the game creates, never per frame).
class ShaderFixStats final {
public:
    explicit ShaderFixStats(diag::ports::Log& log) noexcept : log_(log) {}

    // One more shader created; returns the total.
    int count_created() noexcept { return ++created_; }
    // The fixed shader of creation `total` was accepted.
    void count_fixed(int total) noexcept;
    // The driver refused a fixed shader with `hr`; the original is used.
    void count_rejected(long hr) noexcept;

private:
    diag::ports::Log& log_;
    std::atomic<int> created_{0};
    std::atomic<int> fixed_{0};
    std::atomic<int> rejected_{0};
};

} // namespace omvr::core::render
