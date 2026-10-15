#pragma once

#include "game/addresses.h"

namespace omvr::game::audio {

// The game reopens its audio output (AI.exe table slot AudioReopenOutput),
// after its entry was checked. False, nothing called, when the entry is not
// the expected code (another build, or the unverified 2020 address).
// `addresses` must be initialised. The thread that starts the runtime.
// From: FUN_100037b0 (the caller).
bool reopen_output(const AddressMap& addresses) noexcept;

} // namespace omvr::game::audio
