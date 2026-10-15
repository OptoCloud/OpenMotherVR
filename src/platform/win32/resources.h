#pragma once

#include <cstddef>
#include <span>

namespace omvr::platform::win32 {

// The bytes of the BINARY resource `id` of `module` (an HMODULE), empty when
// it is missing. Valid for the module's lifetime.
std::span<const std::byte> resource_bytes(void* module, int id) noexcept;

} // namespace omvr::platform::win32
