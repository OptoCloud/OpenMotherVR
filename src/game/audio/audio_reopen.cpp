#include "game/audio/audio_reopen.h"

#include "game/code_address.h"
#include "game/x86.h"

#include <array>
#include <cstdint>

namespace omvr::game::audio {

namespace {

// The flag value the entry stores before it calls the getter.
constexpr std::uint8_t kReopenFlagValue = 1;

// The reopen function's entry as verified on the 2015 build:
// `mov byte [flag], 1` (C6 05 <abs32> 01), then `call` (E8) the engine's
// "initialised" getter. The absolute address in between is masked: it
// moves with the image. Anything else is not the function this table
// entry names (another build, or the unverified 2020 address), and calling
// it would run arbitrary game code.
constexpr x86::Pattern<8> kReopenEntry{
    {x86::kMovRm8Imm8, x86::kModAbsolute, 0, 0, 0, 0, kReopenFlagValue, x86::kCallRel32},
    {0xFF, 0xFF, 0, 0, 0, 0, 0xFF, 0xFF}};

// The reopen function's type: no arguments, nothing returned.
using ReopenFn = void(__cdecl*)();

} // namespace

bool reopen_output(const AddressMap& addresses) noexcept {
    // Its address is in the table for both builds; an RVA of 0 (unknown)
    // gives the image base, whose bytes ("MZ") do not match.
    std::uint8_t* reopen = addresses.at(AiAddr::AudioReopenOutput);
    if (!reopen || !x86::matches(reopen, kReopenEntry)) return false;
    const ReopenFn call = CodeAddress{reopen};
    call();
    return true;
}

} // namespace omvr::game::audio
