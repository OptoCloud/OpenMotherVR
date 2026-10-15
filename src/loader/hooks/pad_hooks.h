#pragma once

#include "game/sites/controls.h"

namespace omvr::loader::hooks {

// The replacements for the game pad's five methods and the left-stick
// prompt check.
game::sites::PadReplacements pad_replacements() noexcept;

} // namespace omvr::loader::hooks
