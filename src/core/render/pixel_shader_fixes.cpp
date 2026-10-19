// OpenMotherVR-Provenance: clean
#include "core/render/pixel_shader_fixes.h"

#include "core/render/dither_fix.h"

#include <vector>

namespace omvr::core::render {

namespace {

// SUCCEEDED of the Windows SDK: a negative HRESULT is a failure.
constexpr bool succeeded(long hr) noexcept { return hr >= 0; }

} // namespace

long create_fixed_pixel_shader(std::span<const std::uint8_t> code, ShaderFixStats& dither_stats,
                               ConstantSmoke& smoke, PixelShaderDevice& device) noexcept {
    const int total   = dither_stats.count_created();
    const auto dither = patch_position_dither(code.data(), code.size());
    if (dither.blob.empty() && dither.has_constants) dither_stats.count_unchanged(total);
    const std::span<const std::uint8_t> fixed = dither.blob.empty() ? code : std::span(dither.blob);

    // 24 §15: on top of the dither fix, which changes other instructions.
    if (const auto brightness = smoke.brightness()) {
        auto& smoke_stats                    = smoke.stats();
        const int smoke_total                = smoke_stats.count_created();
        const std::vector<std::uint8_t> both = patch_constant_smoke(fixed.data(), fixed.size(), *brightness);
        if (!both.empty()) {
            const long hr = device.create(both);
            if (succeeded(hr)) {
                smoke_stats.count_fixed(smoke_total);
                if (!dither.blob.empty()) dither_stats.count_fixed(total);
                return hr;
            }
            smoke_stats.count_rejected(hr);
        }
    }
    if (!dither.blob.empty()) {
        const long hr = device.create(dither.blob);
        if (succeeded(hr)) {
            dither_stats.count_fixed(total);
            return hr;
        }
        dither_stats.count_rejected(hr);
    }
    return device.create(code);
}

} // namespace omvr::core::render
