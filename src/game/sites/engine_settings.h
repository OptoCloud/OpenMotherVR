// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "game/game_image.h"

#include <string_view>

namespace omvr::game::sites {

// The log area of the engine settings override (24 §11.3).
inline constexpr std::string_view kEngineSettingsArea = "engine settings";

// The path the game opens its engine settings file by (24 §1.9), read from
// AI.exe's own pointer once the load site is the expected code: `mov eax,
// [path pointer]; push 1; push eax; lea ecx, [esp+0x38]; call XmlLoad`.
// Empty, and logged, when this build has no addresses for it or the code
// differs; the override then stays off. Nothing is written.
std::string_view engine_settings_path(const GameImage& g, core::diag::ports::Log& log) noexcept;

} // namespace omvr::game::sites
