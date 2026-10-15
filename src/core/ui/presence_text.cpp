// OpenMotherVR-Provenance: clean
#include "core/ui/presence_text.h"

#include <cstddef>

namespace omvr::core::ui {

namespace {

constexpr std::string_view kMainMenu   = "Main menu";
constexpr std::string_view kNotTracked = "VR off";

std::string_view doing(input::Interaction i) noexcept {
    using input::Interaction;
    switch (i) {
        case Interaction::Hacking: return "Hacking a lock";
        case Interaction::Cutting: return "Cutting through a panel";
        case Interaction::Terminal: return "Reading a terminal";
        case Interaction::Keypad: return "Entering a keycode";
        case Interaction::Rewire: return "Rewiring the power";
        case Interaction::Ladder: return "Climbing a ladder";
        case Interaction::SuitChange: return "Putting on a spacesuit";
        case Interaction::Generic:
        case Interaction::None: break;
    }
    return {};
}

} // namespace

PresenceText describe(const PresenceInput& in) noexcept {
    if (!in.tracked) return {kNotTracked, {}};
    const bool known             = in.level >= 0 && static_cast<std::size_t>(in.level) < levels().size();
    const bool front             = !known || in.level == 0;
    const std::string_view where = known ? levels()[static_cast<std::size_t>(in.level)].name : kMainMenu;

    if (in.screen == Screen::Loading) return {where, "Loading"};
    if (in.screen == Screen::Title || front) return {kMainMenu, {}};
    if (in.screen == Screen::Pause) return {where, "Paused"};
    if (in.cutscene) return {where, "Watching a cutscene"};
    if (const auto what = doing(in.interaction); !what.empty()) return {where, what};
    if (in.hiding) return {where, "Hiding"};
    return {where, "Exploring"};
}

} // namespace omvr::core::ui
