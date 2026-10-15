#pragma once

#include "game/event_tap.h"

namespace omvr::loader::hooks {

// The script event tap's replacement.
game::TapFn event_tap_replacement() noexcept;

} // namespace omvr::loader::hooks
