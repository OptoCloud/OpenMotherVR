#pragma once

#include "core/diag/ports/log.h"
#include "game/addresses.h"
#include "game/constants.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace omvr::game {

// Identify the loaded AI.exe by its PE timestamp (01 §3.1).
// Returns std::nullopt if the file isn't an AI build we recognize.
std::optional<GameBuild> identify_ai_exe() noexcept;

// Raw PE timestamp of the loaded AI.exe (zero if not found). Useful for
// logging the value when the build is not one we recognize.
std::uint32_t ai_exe_timestamp_raw() noexcept;

// The year a build is known by in the log ("2015", "2020").
std::string_view build_name(GameBuild build) noexcept;

// 1: every address is build-specific, so an unknown build is not touched
// (01 §2.4). Selects and rebases `addresses` for the running build;
// nullopt, logged, for an unknown one.
std::optional<GameBuild> select_address_table(std::uint8_t* ai_base, AddressMap& addresses,
                                              core::diag::ports::Log& log) noexcept;

} // namespace omvr::game
