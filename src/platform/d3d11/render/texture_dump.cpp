#include "platform/d3d11/render/texture_dump.h"

#include "core/diag/logger.h" // R: render (A3): log through an injected Log&
#include "platform/d3d11/render/dds.h"
#include "platform/win32/module.h"
#include "platform/win32/text.h"

#include <d3d11.h>

#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <vector>

namespace omvr::platform::d3d11 {

using core::diag::logf;
using win32::module_directory;
using win32::utf8;

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
            const std::uint32_t magic = kDdsMagic;
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

void dump_bound_target(ID3D11DeviceContext* ctx, int pass) {
    if (!ctx) return;
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) {
        logf("[diag] reflection pass {}: no render target bound", pass);
        return;
    }
    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    dump_texture(ctx, res, std::format("mothervr_reflection_{}.dds", pass));
    res->Release();
    rtv->Release();
}

} // namespace omvr::platform::d3d11
