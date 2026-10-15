#include "core/ui/notices.h"

#include <format>

namespace omvr::core::ui {

std::string openvr_not_showing_game(std::string_view shown) {
    return std::format(
        "SteamVR is not showing the game in the headset{}.\n\n"
        "This is usually SteamVR's Desktop Game Theatre. In Steam, open the "
        "Properties of Alien: Isolation and uncheck 'Use Desktop Game Theatre while "
        "SteamVR is active', then restart SteamVR and the game.",
        shown.empty() ? std::string{} : " (it shows " + std::string(shown) + ")");
}

std::string_view openxr_not_showing_game() noexcept {
    return "The OpenXR runtime is not showing the game in the headset.\n\n"
           "With SteamVR this is usually its Desktop Game Theatre. In Steam, open the Properties "
           "of Alien: Isolation and uncheck 'Use Desktop Game Theatre while SteamVR is active', "
           "then restart SteamVR and the game.";
}

} // namespace omvr::core::ui
