#include "core/stereo/sensor_start.h"

#include "game/stereo/graphics_settings.h" // R: stereo (A3): the options as a value list for the thunk

namespace omvr::core::stereo {

void apply_sensor_start_options() noexcept {
    using game::stereo::set_graphics_option;
    set_graphics_option(0x00, "Off");
    set_graphics_option(0x15, "Off");
    set_graphics_option(0x0D, "Off");
    set_graphics_option(0x07, "On");
}

} // namespace omvr::core::stereo
