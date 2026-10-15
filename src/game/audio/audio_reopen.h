#pragma once

namespace omvr::game::audio {

// The game reopens its audio output (AI.exe table slot AudioReopenOutput),
// after its entry was checked. False, nothing called, when the entry is not
// the expected code (another build, or the unverified 2020 address).
// From: FUN_100037b0 (the caller).
bool reopen_output() noexcept;

} // namespace omvr::game::audio
