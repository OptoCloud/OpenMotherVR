#pragma once

#include "game/addresses.h"
#include "game/constants.h"

#include <cstdint>

namespace omvr::game {

// The running AI.exe as the installer sees it.
struct GameImage {
    std::uint8_t*      base;
    std::uint32_t      image_size;
    GameBuild          version;
    const InlineSites& sites;
};

// Check every site the core steps write, before any of them is written: the
// UI set always, the rest with `full_set`. Opcodes and operands that do not
// depend on the build are checked exactly; call targets where the table
// names the callee or two sites share one; vtable slots against the stock
// method the table names. Each failure is logged with the site's RVA and
// what was found there, so one run names every mismatch. False when any
// failed: nothing may be patched then.
bool preflight(const GameImage& g, const AddressMap& addresses, bool full_set) noexcept;

} // namespace omvr::game
