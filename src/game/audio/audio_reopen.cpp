#include "game/audio/audio_reopen.h"

#include "game/code_address.h"
#include "game/x86.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace omvr::game::audio {

namespace {

// The flag value the entry stores before it calls the getter.
constexpr std::uint8_t kReopenFlagValue = 1;

// One byte of a code pattern at its offset from the entry.
struct PatternByte {
    std::size_t  offset;
    std::uint8_t value;
};

// The reopen function's entry as verified on the 2015 build:
// `mov byte [flag], 1` (C6 05 <abs32> 01), then `call` (E8) the engine's
// "initialised" getter. The absolute address in between is not compared:
// it moves with the image. Anything else is not the function this table
// entry names (another build, or the unverified 2020 address), and calling
// it would run arbitrary game code.
constexpr std::size_t kImm8Offset = 2 + x86::kRel32Size; // opcode, ModRM, disp32
constexpr std::array<PatternByte, 4> kReopenEntry{{
    {0, x86::kMovRm8Imm8},
    {1, x86::kModAbsolute},
    {kImm8Offset, kReopenFlagValue},
    {kImm8Offset + 1, x86::kCallRel32},
}};

bool reopen_entry_matches(const std::uint8_t* p) noexcept {
    for (const PatternByte& b : kReopenEntry) {
        if (p[b.offset] != b.value) return false;
    }
    return true;
}

// The reopen function's type: no arguments, nothing returned.
using ReopenFn = void(__cdecl*)();

} // namespace

bool reopen_output(const AddressMap& addresses) noexcept {
    // Its address is in the table for both builds; an RVA of 0 (unknown)
    // gives the image base, whose bytes ("MZ") do not match.
    std::uint8_t* reopen = addresses.at(AiAddr::AudioReopenOutput);
    if (!reopen || !reopen_entry_matches(reopen)) return false;
    const ReopenFn call = CodeAddress{reopen};
    call();
    return true;
}

} // namespace omvr::game::audio
