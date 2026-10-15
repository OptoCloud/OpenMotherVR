// OpenMotherVR-Provenance: clean
#pragma once

#include <string>
#include <string_view>

// Notices to the player while the game runs.
namespace omvr::core::ui {

// SteamVR shows another application in the headset (`shown`, the process
// name, or empty when unknown); usually its Desktop Game Theatre.
std::string openvr_not_showing_game(std::string_view shown);

// The OpenXR runtime never made the session visible.
std::string_view openxr_not_showing_game() noexcept;

} // namespace omvr::core::ui
