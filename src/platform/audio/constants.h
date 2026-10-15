#pragma once

#include <cstddef>

namespace omvr::platform::audio {

// IMMDevice::Activate is slot 3 of the vtable every MMDevAPI device shares.
inline constexpr std::size_t kActivateSlot = 3;

} // namespace omvr::platform::audio
