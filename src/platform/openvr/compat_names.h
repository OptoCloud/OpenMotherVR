#pragma once

// Names SteamVR and players know the mod by, kept as the original has them.
namespace omvr::platform::openvr {

// The key and name of the virtual-screen overlay.
inline constexpr const char* kOverlayKey = "MotherVROverlay";

// The folder next to the game holding the action manifest and bindings.
inline constexpr const char* kInputDirectory = "mothervr_input";

// The start of each default binding's name in SteamVR's binding UI.
inline constexpr const char* kBindingNamePrefix = "MotherVR revived: ";

} // namespace omvr::platform::openvr
