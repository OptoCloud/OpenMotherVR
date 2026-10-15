// OpenMotherVR-Provenance: clean
#include "core/render/shader_fix_stats.h"

#include "core/render/tuning.h"

#include <string_view>

namespace omvr::core::render {

namespace {

constexpr std::string_view kStatsLog = "proxy: pixel shaders: {} created, {} dither-fixed, {} rejected";
constexpr std::string_view kRejectedLog =
    "proxy: patched pixel shader rejected (hr=0x{:08X}), using the original";

} // namespace

void ShaderFixStats::count_fixed(int total) noexcept {
    const int n = ++fixed_;
    if (n == 1 || n % kShaderStatsLogPeriod == 0)
        diag::ports::logf(log_, kStatsLog, total, n, rejected_.load());
}

void ShaderFixStats::count_rejected(long hr) noexcept {
    if (++rejected_ <= kShaderRejectLogLimit)
        diag::ports::logf(log_, kRejectedLog, static_cast<unsigned>(hr));
}

} // namespace omvr::core::render
