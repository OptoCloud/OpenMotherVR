// OpenMotherVR-Provenance: clean
#include "core/render/shader_fix_stats.h"

#include "core/render/tuning.h"

#include <string_view>

namespace omvr::core::render {

namespace {

// 11 §2.2 r8.
constexpr std::string_view kStatsLog =
    "proxy: Dither fix: {1} of {0} pixel shaders changed so far; {2} with the "
    "dither's constants left unchanged; {3} refused by the device";
constexpr std::string_view kRejectedLog =
    "proxy: patched pixel shader rejected (hr=0x{:08X}), using the original";
// 24 §14.7.
constexpr std::string_view kSpriteStatsLog =
    "proxy: upright sprites: {1} of {0} vertex shaders changed so far; {2} refused by the device";
constexpr std::string_view kSpriteRejectedLog =
    "proxy: upright-sprite vertex shader refused by the device (hr=0x{:08X}), using the original";

// 24 §15.6.
constexpr std::string_view kSmokeStatsLog =
    "proxy: Constant smoke lighting: {1} of {0} pixel shaders changed so far; {2} refused by the device";
constexpr std::string_view kSmokeRejectedLog =
    "proxy: constant smoke lighting pixel shader refused by the device (hr=0x{:08X}), using the original";

} // namespace

void ShaderFixStats::log_counts(int total, int fixed) noexcept {
    switch (fix_) {
        case ShaderFix::Dither:
            diag::ports::logf(log_, kStatsLog, total, fixed, unchanged_.load(), rejected_.load());
            break;
        case ShaderFix::UprightSprites:
            diag::ports::logf(log_, kSpriteStatsLog, total, fixed, rejected_.load());
            break;
        case ShaderFix::ConstantSmoke:
            diag::ports::logf(log_, kSmokeStatsLog, total, fixed, rejected_.load());
            break;
    }
}

void ShaderFixStats::count_fixed(int total) noexcept {
    const int n = ++fixed_;
    if (n == 1 || n % kShaderStatsLogPeriod == 0) log_counts(total, n);
}

// Logged at the first, so a session with any such shader shows it early,
// and then at the rate of the fixed ones.
void ShaderFixStats::count_unchanged(int total) noexcept {
    const int n = ++unchanged_;
    if (n == 1 || n % kShaderStatsLogPeriod == 0) log_counts(total, fixed_.load());
}

void ShaderFixStats::count_rejected(long hr) noexcept {
    if (++rejected_ <= kShaderRejectLogLimit) {
        const auto code = static_cast<unsigned>(hr);
        switch (fix_) {
            case ShaderFix::Dither: diag::ports::logf(log_, kRejectedLog, code); break;
            case ShaderFix::UprightSprites: diag::ports::logf(log_, kSpriteRejectedLog, code); break;
            case ShaderFix::ConstantSmoke: diag::ports::logf(log_, kSmokeRejectedLog, code); break;
        }
    }
}

} // namespace omvr::core::render
