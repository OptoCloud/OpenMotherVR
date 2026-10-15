#include "core/render/shader_fix_stats.h"

#include "core/diag/logger.h"
#include "core/render/tuning.h"

namespace omvr::core::render {

void ShaderFixStats::count_fixed(int total) noexcept {
    const int n = ++g_ps_patched;
    if (n == 1 || n % kShaderStatsLogPeriod == 0) {
        diag::logf("proxy: pixel shaders: {} created, {} dither-fixed, {} rejected",
                   total, n, g_ps_rejected.load());
    }
}

void ShaderFixStats::count_rejected(long hr) noexcept {
    if (++g_ps_rejected <= kShaderRejectLogLimit) {
        diag::logf("proxy: patched pixel shader rejected (hr=0x{:08X}), using the original",
                   static_cast<unsigned>(hr));
    }
}

} // namespace omvr::core::render
