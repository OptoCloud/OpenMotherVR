#pragma once

#include <filesystem>

struct ID3D11DeviceContext;
struct ID3D11Resource;

// Debug: write mip 0 of a 2D texture to a .dds file (DX10 header, the
// texture's own format). scripts/dds2png.py converts it for viewing.
namespace mothervr::diag {

bool dump_texture(ID3D11DeviceContext* ctx, ID3D11Resource* texture,
                  const std::filesystem::path& path) noexcept;

} // namespace mothervr::diag
