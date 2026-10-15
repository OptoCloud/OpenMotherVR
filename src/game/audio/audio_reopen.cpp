#include "game/audio/audio_reopen.h"

#include "game/addresses.h"

#include <cstdint>

namespace omvr::game::audio {

namespace {

// The reopen function's entry as verified on the 2015 build:
// `mov byte [flag], 1` (C6 05 <abs32> 01), then `call` (E8) the engine's
// "initialised" getter. Anything else is not the function this table entry
// names (another build, or the unverified 2020 address), and calling it
// would run arbitrary game code.
bool reopen_entry_matches(const std::uint8_t* p) noexcept {
    return p[0] == 0xC6 && p[1] == 0x05 && p[6] == 0x01 && p[7] == 0xE8;
}

} // namespace

bool reopen_output() noexcept {
    // The game reopens its audio output, now through the hook. Its address
    // is in the table for both builds; an RVA of 0 (unknown) gives the image
    // base, whose bytes ("MZ") do not match.
    std::uint8_t* reopen = ai_address(AiAddr::AudioReopenOutput);
    if (!reopen || !reopen_entry_matches(reopen)) return false;
    reinterpret_cast<void(__cdecl*)()>(reopen)();
    return true;
}

} // namespace omvr::game::audio
