#pragma once

#include <atomic>

namespace omvr::core::render {

// Counts the pixel shaders the game creates and the ones the dither fix
// rewrote, and logs them at a low rate. Any thread that creates shaders.
class ShaderFixStats {
public:
    // One more shader created; returns the total.
    int count_created() noexcept { return ++g_ps_total; }
    // The fixed shader of creation `total` was accepted.
    void count_fixed(int total) noexcept;
    // The driver refused a fixed shader with `hr`; the original is used.
    void count_rejected(long hr) noexcept;

private:
    std::atomic<int> g_ps_total{0}, g_ps_patched{0}, g_ps_rejected{0};
};

} // namespace omvr::core::render
