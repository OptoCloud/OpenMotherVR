#include "platform/win32/resources.h"

#include "platform/win32/constants.h"

#include <windows.h>

namespace omvr::platform::win32 {

std::span<const std::byte> resource_bytes(void* module, int id) noexcept {
    const auto self = static_cast<HMODULE>(module);
    HRSRC info      = ::FindResourceW(self, MAKEINTRESOURCEW(id), kBinaryResourceType);
    if (!info) return {};
    HGLOBAL data = ::LoadResource(self, info);
    if (!data) return {};
    return {static_cast<const std::byte*>(::LockResource(data)), ::SizeofResource(self, info)};
}

} // namespace omvr::platform::win32
