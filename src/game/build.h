#pragma once

#include "game/addresses.h"
#include "game/constants.h"

#include <cstdint>
#include <optional>

namespace omvr::game {

// Identify the loaded AI.exe by its PE timestamp.
// Returns std::nullopt if the file isn't an AI build we recognize.
std::optional<GameBuild> identify_ai_exe();

// Raw PE timestamp of the loaded AI.exe (zero if not found). Useful for
// logging the value when the build is not one we recognize.
std::uint32_t ai_exe_timestamp_raw() noexcept;

// 1: the original refuses to touch an unknown build ("Exe check failed!"):
// every address is build-specific. Selects and rebases `addresses` for the
// running build; nullopt, logged, for an unknown one.
std::optional<GameBuild> select_address_table(std::uint8_t* ai_base, AddressMap& addresses) noexcept;

} // namespace omvr::game
