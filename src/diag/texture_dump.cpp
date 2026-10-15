#include "texture_dump.h"

#include "log.h"

#include <d3d11.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace mothervr::diag {

namespace {

// DDS with the DX10 extension header: the format travels as a DXGI_FORMAT.
struct DdsPixelFormat {
    std::uint32_t size = 32, flags = 0x4 /*FOURCC*/, four_cc = 0x30315844 /*"DX10"*/;
    std::uint32_t rgb_bits = 0, r = 0, g = 0, b = 0, a = 0;
};
struct DdsHeader {
    std::uint32_t size = 124;
    std::uint32_t flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x8;  // caps, height, width, pixelformat, pitch
    std::uint32_t height = 0, width = 0, pitch = 0, depth = 0, mip_count = 1;
    std::uint32_t reserved[11]{};
    DdsPixelFormat format{};
    std::uint32_t caps = 0x1000, caps2 = 0, caps3 = 0, caps4 = 0, reserved2 = 0;
};
struct DdsHeaderDx10 {
    std::uint32_t dxgi_format = 0, dimension = 3 /*TEXTURE2D*/, misc = 0, array_size = 1, misc2 = 0;
};
static_assert(sizeof(DdsHeader) == 124 && sizeof(DdsHeaderDx10) == 20);

} // namespace

bool dump_texture(ID3D11DeviceContext* ctx, ID3D11Resource* texture,
                  const std::filesystem::path& path) noexcept {
    if (!ctx || !texture) return false;
    std::filesystem::path target;
    try {
        target = path.is_relative() ? module_directory() / path : path;
    } catch (...) {
        return false;
    }
    ID3D11Texture2D* src = nullptr;
    if (FAILED(texture->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&src)))) return false;

    D3D11_TEXTURE2D_DESC desc{};
    src->GetDesc(&desc);
    ID3D11Device* device = nullptr;
    ctx->GetDevice(&device);

    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.SampleDesc = {1, 0};
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    bool ok = false;
    if (desc.SampleDesc.Count == 1 && SUCCEEDED(device->CreateTexture2D(&staging_desc, nullptr, &staging))) {
        ctx->CopySubresourceRegion(staging, 0, 0, 0, 0, src, 0, nullptr);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            DdsHeader header{};
            header.width  = desc.Width;
            header.height = desc.Height;
            header.pitch  = mapped.RowPitch;
            DdsHeaderDx10 dx10{};
            dx10.dxgi_format = static_cast<std::uint32_t>(desc.Format);
            // A bare name goes to the game folder: the current directory is
            // wherever Steam started the game.
            std::ofstream out(target, std::ios::binary);
            const std::uint32_t magic = 0x20534444;  // "DDS "
            out.write(reinterpret_cast<const char*>(&magic), 4);
            out.write(reinterpret_cast<const char*>(&header), sizeof(header));
            out.write(reinterpret_cast<const char*>(&dx10), sizeof(dx10));
            for (UINT y = 0; y < desc.Height; ++y) {
                out.write(static_cast<const char*>(mapped.pData) + std::size_t(y) * mapped.RowPitch,
                          mapped.RowPitch);
            }
            ctx->Unmap(staging, 0);
            ok = out.good();
        }
        staging->Release();
    }
    logf("[diag] dump {}x{} format {} -> {} ({})", desc.Width, desc.Height,
         static_cast<int>(desc.Format), utf8(target), ok ? "ok" : "failed");
    device->Release();
    src->Release();
    return ok;
}

} // namespace mothervr::diag
