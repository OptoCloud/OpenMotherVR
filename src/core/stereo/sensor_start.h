#pragma once

#include "core/stereo/types.h"

#include <array>
#include <cstdint>

// The stereo mode event (02 §6.5): the game's setting 0x18 "Stereo Mode"
// reaches the manager's OnEvent; with a device attached it starts the sensor
// and switches graphics settings for stereo.
namespace omvr::core::stereo {

// One setting change, in the game's own spelling of the value.
struct GraphicsChange {
    GraphicsOption option;
    const char* value;
};

// The changes made when the sensor starts, in this order. Depth of field
// and motion blur are unpleasant in a headset, SSAO is wrong across a
// side-by-side target. VSync "On" is kept as the mod has always set it,
// although 02 §6.5 leaves VSync to the player.
// From: the StereoManager vtable 0x1005a61c, slot 1 (FUN_10020230).
inline constexpr std::array<GraphicsChange, 4> kSensorStartChanges = {{
    {GraphicsOption::DepthOfField, "Off"},
    {GraphicsOption::Ssao, "Off"},
    {GraphicsOption::MotionBlur, "Off"},
    {GraphicsOption::VSync, "On"},
}};

// The event with setting id `id` starts the sensor.
constexpr bool starts_sensor(std::int32_t id) noexcept {
    return id == static_cast<std::int32_t>(GraphicsOption::StereoMode);
}

} // namespace omvr::core::stereo
