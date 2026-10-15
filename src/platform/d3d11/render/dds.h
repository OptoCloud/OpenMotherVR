#pragma once

#include <cstdint>

// The DDS file format with the DX10 extension header: the format travels as
// a DXGI_FORMAT.
namespace omvr::platform::d3d11 {

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

// "DDS ", the file's first four bytes.
inline constexpr std::uint32_t kDdsMagic = 0x20534444;

} // namespace omvr::platform::d3d11
