// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "game/game_image.h"

#include <cstdint>

namespace omvr::game::input {

// The game's window-focus byte (ReconRvas::window_focus), or null when this
// build has no address for it or its clear in the window procedure is not
// the expected code. Logs which.
[[nodiscard]] std::uint8_t* find_window_focus(const GameImage& g, core::diag::ports::Log& log) noexcept;

// What the window procedure's WM_KILLFOCUS branch does before it posts the
// pause event: the game drops background keyboard and mouse input until
// WM_SETFOCUS sets the byte again. Game thread.
void clear_window_focus(std::uint8_t* focus) noexcept;

} // namespace omvr::game::input
