// OpenMotherVR-Provenance: clean
#pragma once

#include "core/stereo/types.h"
#include "game/addresses.h"

// The game's graphics settings object (02 §6.5).
namespace omvr::game::stereo {

// settings.SetOption(id, value), the game's own: thiscall, ret 8. `value`
// is the game's spelling ("Off", "On").
void set_graphics_option(const AddressMap& addresses, core::stereo::GraphicsOption option,
                         const char* value) noexcept;

} // namespace omvr::game::stereo
