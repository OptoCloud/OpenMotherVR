// OpenMotherVR-Provenance: clean
#include "core/hands/activity.h"

namespace omvr::core::hands {

Activity activity(const ActivityInput& in) noexcept {
    if (!in.enabled) return Activity::Off;
    if (!in.stereo_running) return Activity::NoStereo;
    if (!in.world_mode) return Activity::Screen;
    if (in.menu_screen) return Activity::MenuScreen;
    if (!in.player) return Activity::NoPlayer;
    if (!in.alive) return Activity::Dead;
    if (in.cutscene_or_fade) return Activity::Cutscene;
    if (!in.follow_active) return Activity::CameraTaken;
    if (in.interaction) return Activity::Interaction;
    if (in.hiding) return Activity::Hiding;
    if (in.player_action) return Activity::PlayerAction;
    return Activity::Active;
}

std::string_view to_string(Activity a) noexcept {
    switch (a) {
        case Activity::Active: return "active";
        case Activity::Off: return "Hands.Enabled off";
        case Activity::NoStereo: return "no stereo device";
        case Activity::Screen: return "virtual screen";
        case Activity::NoPlayer: return "no player character";
        case Activity::Dead: return "player dead";
        case Activity::CameraTaken: return "camera taken";
        case Activity::Cutscene: return "cutscene or level fade";
        case Activity::MenuScreen: return "menu or loading screen";
        case Activity::Interaction: return "interaction";
        case Activity::Hiding: return "hiding";
        case Activity::PlayerAction: return "player action";
    }
    return "unknown";
}

bool is_episode(Activity a) noexcept {
    switch (a) {
        case Activity::CameraTaken:
        case Activity::Cutscene:
        case Activity::MenuScreen:
        case Activity::Interaction:
        case Activity::Hiding:
        case Activity::PlayerAction: return true;
        default: return false;
    }
}

} // namespace omvr::core::hands
