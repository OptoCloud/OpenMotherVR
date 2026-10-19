// OpenMotherVR-Provenance: clean
#include "core/ui/camera_takeovers.h"

namespace omvr::core::ui {

std::string_view takeover_name(Takeover kind) noexcept {
    switch (kind) {
        case Takeover::StealCameraSteer: return "StealCamera steer";
        case Takeover::LevelCameraExitSteer: return "level camera exit steer";
        case Takeover::LevelCameraExitRealign: return "level camera exit re-align";
        case Takeover::ResetRealign: return "camera reset re-align";
        case Takeover::StateEntryRealign: return "state entry re-align";
    }
    return "takeover";
}

void CameraTakeoverRule::observe(const TakeoverInputs& in) noexcept {
    if (!in.in_interaction) {
        interaction_rule_.reset();
        return;
    }
    if (!interaction_rule_) interaction_rule_ = in.setting;
}

config::CameraTakeovers CameraTakeoverRule::rule(const TakeoverInputs& in) const noexcept {
    return in.in_interaction && interaction_rule_ ? *interaction_rule_ : in.setting;
}

bool CameraTakeoverRule::suppress(const TakeoverInputs& in) noexcept {
    observe(in);
    // 23 §4.4: during a cutscene or level fade every takeover is the game's.
    if (in.cutscene_or_fade) return false;
    return rule(in) == config::CameraTakeovers::Reduced;
}

} // namespace omvr::core::ui
