#include "platform/d3d11/render/texture_dump.h"

#include "platform/d3d11/render/dds.h"
#include "platform/win32/com_ptr.h"
#include "platform/win32/module.h"
#include "platform/win32/text.h"

#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <optional>
#include <string_view>

namespace omvr::platform::d3d11 {

namespace {

using win32::ComPtr;

constexpr std::string_view kDumpLog       = "[diag] dump {}x{} format {} -> {} ({})";
constexpr std::string_view kNoTargetLog   = "[diag] reflection pass {}: no render target bound";
constexpr std::string_view kReflectionDds = "mothervr_reflection_{}.dds";

// The bytes of a plain value, for the file. The one cast from an object to
// its bytes in this file.
template <class T>
void write_value(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

// A CPU-readable single-sample copy of mip 0 of `desc`'s texture.
D3D11_TEXTURE2D_DESC staging_desc_of(const D3D11_TEXTURE2D_DESC& desc) noexcept {
    D3D11_TEXTURE2D_DESC staging = desc;
    staging.MipLevels      = 1;
    staging.ArraySize      = 1;
    staging.SampleDesc     = {1, 0};
    staging.Usage          = D3D11_USAGE_STAGING;
    staging.BindFlags      = 0;
    staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging.MiscFlags      = 0;
    return staging;
}

// Writes the mapped rows of a `desc` texture as a DDS file.
bool write_dds(const std::filesystem::path& target, const D3D11_TEXTURE2D_DESC& desc,
               const D3D11_MAPPED_SUBRESOURCE& mapped) {
    DdsHeader header{};
    header.width  = desc.Width;
    header.height = desc.Height;
    header.pitch  = mapped.RowPitch;
    DdsHeaderDx10 dx10{};
    dx10.dxgi_format = static_cast<std::uint32_t>(desc.Format);
    std::ofstream out(target, std::ios::binary);
    write_value(out, kDdsMagic);
    write_value(out, header);
    write_value(out, dx10);
    for (UINT y = 0; y < desc.Height; ++y) {
        out.write(static_cast<const char*>(mapped.pData) + std::size_t(y) * mapped.RowPitch, mapped.RowPitch);
    }
    return out.good();
}

// Copies mip 0 into a staging texture and writes it; false if any step
// failed (a multisampled texture is not copied).
bool copy_and_write(ID3D11DeviceContext* ctx, ID3D11Device* device, ID3D11Texture2D* src,
                    const D3D11_TEXTURE2D_DESC& desc, const std::filesystem::path& target) {
    if (desc.SampleDesc.Count != 1) return false;
    const D3D11_TEXTURE2D_DESC staging_desc = staging_desc_of(desc);
    ID3D11Texture2D* raw_staging = nullptr;
    if (FAILED(device->CreateTexture2D(&staging_desc, nullptr, &raw_staging))) return false;
    const auto staging = ComPtr<ID3D11Texture2D>::adopt(raw_staging);
    ctx->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, src, 0, nullptr);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(ctx->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;
    const bool ok = write_dds(target, desc, mapped);
    ctx->Unmap(staging.get(), 0);
    return ok;
}

std::optional<std::filesystem::path> resolve(const std::filesystem::path& path) noexcept {
    try {
        return path.is_relative() ? win32::module_directory() / path : path;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

bool dump_texture(ID3D11DeviceContext* ctx, ID3D11Resource* texture, const std::filesystem::path& path,
                  core::diag::ports::Log& log) noexcept {
    if (!ctx || !texture) return false;
    const auto target = resolve(path);
    if (!target) return false;
    ID3D11Texture2D* raw_src = nullptr;
    if (FAILED(texture->QueryInterface(IID_PPV_ARGS(&raw_src)))) return false;
    const auto src = ComPtr<ID3D11Texture2D>::adopt(raw_src);

    D3D11_TEXTURE2D_DESC desc{};
    src->GetDesc(&desc);
    ID3D11Device* raw_device = nullptr;
    ctx->GetDevice(&raw_device);
    const auto device = ComPtr<ID3D11Device>::adopt(raw_device);

    bool ok = false;
    try {
        ok = copy_and_write(ctx, device.get(), src.get(), desc, *target);
        core::diag::ports::logf(log, kDumpLog, desc.Width, desc.Height, static_cast<int>(desc.Format),
                                win32::utf8(*target), ok ? "ok" : "failed");
    } catch (...) {
        ok = false;
    }
    return ok;
}

void dump_bound_target(ID3D11DeviceContext* ctx, int pass, core::diag::ports::Log& log) {
    if (!ctx) return;
    ID3D11RenderTargetView* raw_rtv = nullptr;
    ctx->OMGetRenderTargets(1, &raw_rtv, nullptr);
    if (!raw_rtv) {
        core::diag::ports::logf(log, kNoTargetLog, pass);
        return;
    }
    const auto rtv = ComPtr<ID3D11RenderTargetView>::adopt(raw_rtv);
    ID3D11Resource* raw_res = nullptr;
    rtv->GetResource(&raw_res);
    const auto res = ComPtr<ID3D11Resource>::adopt(raw_res);
    dump_texture(ctx, res.get(), std::format(kReflectionDds, pass), log);
}

} // namespace omvr::platform::d3d11
