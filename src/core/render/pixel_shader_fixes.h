// OpenMotherVR-Provenance: clean
#pragma once

#include "core/render/constant_smoke.h"
#include "core/render/shader_fix_stats.h"

#include <cstdint>
#include <span>

// The fixes of the game's pixel shaders applied together when the game
// creates one (08 §1, 24 §15): which blobs are tried in which order, and
// what each outcome counts. Pure apart from the device call it is handed;
// runs on whichever thread creates the shader.
namespace omvr::core::render {

// The device's CreatePixelShader for one blob: its HRESULT, negative when
// the device refused it.
class PixelShaderDevice {
public:
    virtual long create(std::span<const std::uint8_t> blob) noexcept = 0;

protected:
    ~PixelShaderDevice() = default;
};

// Creates the pixel shader `code` on `device` with the fixes on, and
// returns the HRESULT of the shader the game gets. The dither fix comes
// first; with constant smoke on, its fix is layered on the dithered blob
// (it changes other instructions). The most-fixed blob is tried first:
// smoke over dither, then the dither alone, then the original, each after
// the device refused the one before. `dither_stats` and the stats of
// `smoke` count what was created, fixed, left unchanged and refused.
long create_fixed_pixel_shader(std::span<const std::uint8_t> code, ShaderFixStats& dither_stats,
                               ConstantSmoke& smoke, PixelShaderDevice& device) noexcept;

} // namespace omvr::core::render
