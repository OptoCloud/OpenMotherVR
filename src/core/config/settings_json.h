#pragma once

#include "core/config/settings.h"

#include <nlohmann/json.hpp>

#include <string>

// mothervr.json as JSON: the original's layout and key names.
namespace omvr::core::config {

// The settings in `root`, read per key: missing keys and values of the wrong
// type take their defaults one by one.
Settings read_config(const nlohmann::json& root);

// Writes `c` into `root` in the original's layout, keeping other keys.
void write_config(nlohmann::json& root, const Settings& c);

// Every setting under its mothervr.json key path ("Rendering.PixelScale",
// ...), as one line of JSON for the log.
std::string describe(const Settings& cfg);

} // namespace omvr::core::config
