// OpenMotherVR-Provenance: clean
#pragma once

#include "core/stereo/types.h"
#include "game/addresses.h"

// The name hashes of the UI entity messages a cutscene or level fade sends
// (05 §6.2), as the game stores them.
namespace omvr::game::stereo {

core::stereo::SceneMessages scene_messages(const AddressMap& addresses) noexcept;

} // namespace omvr::game::stereo
