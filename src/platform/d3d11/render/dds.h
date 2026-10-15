#pragma once

#include <cstdint>

// The DDS file format with the DX10 extension header (Microsoft's DDS
// documentation): the format travels as a DXGI_FORMAT.
namespace omvr::platform::d3d11 {

// "DDS ", the file's first four bytes.
inline constexpr std::uint32_t kDdsMagic = 0x20534444;

// DDS_HEADER.dwFlags.
inline constexpr std::uint32_t kDdsdCaps        = 0x1;
inline constexpr std::uint32_t kDdsdHeight      = 0x2;
inline constexpr std::uint32_t kDdsdWidth       = 0x4;
inline constexpr std::uint32_t kDdsdPitch       = 0x8;
inline constexpr std::uint32_t kDdsdPixelFormat = 0x1000;
// DDS_HEADER.dwCaps: DDSCAPS_TEXTURE.
inline constexpr std::uint32_t kDdsCapsTexture = 0x1000;
// DDS_PIXELFORMAT.dwFlags: DDPF_FOURCC, and the "DX10" code that says a
// DDS_HEADER_DXT10 follows.
inline constexpr std::uint32_t kDdpfFourCc = 0x4;
inline constexpr std::uint32_t kFourCcDx10 = 0x30315844;
// DDS_HEADER_DXT10.resourceDimension: D3D10_RESOURCE_DIMENSION_TEXTURE2D.
inline constexpr std::uint32_t kDimensionTexture2D = 3;

struct DdsPixelFormat {
    std::uint32_t size = sizeof(DdsPixelFormat), flags = kDdpfFourCc, four_cc = kFourCcDx10;
    std::uint32_t rgb_bits = 0, r = 0, g = 0, b = 0, a = 0;
};
struct DdsHeader {
    std::uint32_t size  = 124;  // sizeof(DdsHeader), asserted below
    std::uint32_t flags = kDdsdCaps | kDdsdHeight | kDdsdWidth | kDdsdPixelFormat | kDdsdPitch;
    std::uint32_t height = 0, width = 0, pitch = 0, depth = 0, mip_count = 1;
    std::uint32_t reserved[11]{};
    DdsPixelFormat format{};
    std::uint32_t caps = kDdsCapsTexture, caps2 = 0, caps3 = 0, caps4 = 0, reserved2 = 0;
};
struct DdsHeaderDx10 {
    std::uint32_t dxgi_format = 0, dimension = kDimensionTexture2D, misc = 0, array_size = 1, misc2 = 0;
};
static_assert(sizeof(DdsPixelFormat) == 32);
static_assert(sizeof(DdsHeader) == 124 && sizeof(DdsHeaderDx10) == 20);

} // namespace omvr::platform::d3d11
