// OpenMotherVR-Provenance: clean
#include "core/stereo/turn_block.h"

#include <string_view>

namespace omvr::core::stereo {

namespace {

using diag::ports::logf;

constexpr std::string_view kSnapLog      = "[input] turning: snap turn, repeat {} ms";
constexpr std::string_view kSnapOnceLog  = "[input] turning: snap turn, one step per push";
constexpr std::string_view kGameLookLog  = "[input] turning: the game's own stick look";
constexpr std::string_view kSmoothLog    = "[input] turning: smooth turn, {} degrees per second";
constexpr std::string_view kBlockedLog   = "[input] turning blocked: {}";
constexpr std::string_view kBlockedKind  = "[input] turning blocked: interaction ({})";
constexpr std::string_view kUnblockedLog = "[input] turning unblocked";

std::string_view interaction_name(input::Interaction kind) noexcept {
    using input::Interaction;
    switch (kind) {
        case Interaction::SuitChange: return "suit change";
        case Interaction::Keypad: return "keypad";
        case Interaction::Hacking: return "hacking";
        case Interaction::Cutting: return "cutting";
        case Interaction::Terminal: return "terminal";
        case Interaction::Ladder: return "ladder";
        case Interaction::Rewire: return "rewire";
        case Interaction::None:
        case Interaction::Generic: break;
    }
    return "generic";
}

std::string_view block_name(TurnBlock block) noexcept {
    switch (block) {
        case TurnBlock::Interaction: return "interaction";
        case TurnBlock::BreathingGame: return "breathing game";
        case TurnBlock::Cutscene: return "cutscene or level fade";
        case TurnBlock::None: break;
    }
    return "none";
}

} // namespace

TurnBlock turn_block(const TurnBlockInput& in) noexcept {
    if (in.in_interaction) return TurnBlock::Interaction;
    if (in.breathing_game) return TurnBlock::BreathingGame;
    if (in.cutscene_or_fade) return TurnBlock::Cutscene;
    return TurnBlock::None;
}

void log_turning(diag::ports::Log& log, const TurningInUse& in_use) noexcept {
    try {
        switch (in_use.turning) {
            case Turning::Snap:
                if (in_use.repeat_ms > 0)
                    logf(log, kSnapLog, in_use.repeat_ms);
                else
                    log.line(kSnapOnceLog);
                break;
            case Turning::GameLook: log.line(kGameLookLog); break;
            case Turning::Smooth: logf(log, kSmoothLog, in_use.speed_deg_s); break;
        }
    } catch (...) {} // a failed format only costs the line
}

void log_turn_block(diag::ports::Log& log, const TurnBlockChange& change) noexcept {
    if (!log.debug_enabled()) return;
    try {
        if (change.block == TurnBlock::None)
            log.line(kUnblockedLog);
        else if (change.block == TurnBlock::Interaction)
            logf(log, kBlockedKind, interaction_name(change.kind));
        else
            logf(log, kBlockedLog, block_name(change.block));
    } catch (...) {}
}

} // namespace omvr::core::stereo
