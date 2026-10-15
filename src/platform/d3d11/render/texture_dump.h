#pragma once

#include <filesystem>

struct ID3D11DeviceContext;
struct ID3D11Resource;

// Debug: write mip 0 of a 2D texture to a .dds file (DX10 header, the
// texture's own format). scripts/dds2png.py converts it for viewing.
namespace omvr::platform::d3d11 {

bool dump_texture(ID3D11DeviceContext* ctx, ID3D11Resource* texture,
                  const std::filesystem::path& path) noexcept;

// Debug (F10): write what the reflection pass left bound as render target
// to <game dir>/mothervr_reflection_<pass>.dds.
void dump_bound_target(ID3D11DeviceContext* ctx, int pass);

} // namespace omvr::platform::d3d11
