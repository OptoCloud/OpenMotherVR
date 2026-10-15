#pragma once

#include "core/diag/ports/log.h"
#include "game/game_image.h"

namespace omvr::game {

// Check every site the core steps write, before any of them is written
// (01 §2.5.1): the UI set always, the rest with `full_set`. Opcodes and
// operands that do not depend on the build are checked exactly; call
// targets where the table names the callee or two sites share one; vtable
// slots against the stock method the table names. Each failure is logged
// with the site's RVA and what was found there, so one run names every
// mismatch. False when any failed: nothing may be patched then.
bool preflight(const GameImage& g, bool full_set, core::diag::ports::Log& log) noexcept;

} // namespace omvr::game
