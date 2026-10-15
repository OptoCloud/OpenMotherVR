// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

#include <cstdint>

namespace omvr::game {

// Puts a gate in front of the game's clear of the bound render target (07):
// while clears are skipped it returns at once (the clear is __cdecl, so the
// caller pops the arguments). The clear's first six bytes are a
// position-independent prologue, which the gate runs before jumping back.
// False if the code at `clear` is not the expected prologue (nothing is
// written).
bool install_clear_gate(std::uint8_t* clear, core::diag::ports::Log& log) noexcept;

// Skip the game's clears (the second eye of the reflection pass). Render
// thread.
void set_skip_clears(bool skip) noexcept;

} // namespace omvr::game
