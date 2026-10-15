// OpenMotherVR-Provenance: clean
#pragma once

#include "game/addresses.h"

namespace omvr::game::input {

// keyboard.KeyDown(scancode), keyboard.AnyKey() and mouse.Active(), the
// game's own (04 §2.2), on the devices its input manager holds. Game thread.
// The scan and pad codes are in game/constants.h.
[[nodiscard]] bool key_down(const AddressMap& addresses, void* keyboard, int scancode) noexcept;
[[nodiscard]] bool any_key(const AddressMap& addresses, void* keyboard) noexcept;
[[nodiscard]] bool mouse_active(const AddressMap& addresses, void* mouse) noexcept;

} // namespace omvr::game::input
