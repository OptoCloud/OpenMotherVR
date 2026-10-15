#pragma once

namespace omvr::core::stereo {

// The manager's event 0x18 starts the sensor; with it four graphics options
// go off or on for stereo.
inline constexpr int kSensorStartEvent = 0x18;

// Switches the four graphics options for stereo.
// From: the StereoManager vtable 0x1005a61c, slot 1 (FUN_10020230).
void apply_sensor_start_options() noexcept;

} // namespace omvr::core::stereo
