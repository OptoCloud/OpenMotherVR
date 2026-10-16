// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/input/interaction_state.h"

#include <cstdint>

// Which turning is in use and why turning is blocked (spec 22 §4.1, §4.4),
// and their log lines (spec 22 §16).
namespace omvr::core::stereo {

// Exactly one applies, chosen every stereo update (spec 22 §4.1).
enum class Turning : std::uint8_t { Snap, GameLook, Smooth };

// The turning in use with its parameter: the repeat interval for snap, the
// speed for smooth.
struct TurningInUse {
    Turning turning                            = Turning::Snap;
    int repeat_ms                              = 0;
    float speed_deg_s                          = 0.0f;
    bool operator==(const TurningInUse&) const = default;
};

// Why `Controls.InteractionAwareTurning` stops all turning now.
enum class TurnBlock : std::uint8_t { None, Interaction, BreathingGame, Cutscene };

// What the block decision reads.
struct TurnBlockInput {
    bool in_interaction                 = false;
    input::Interaction interaction_kind = input::Interaction::None;
    bool breathing_game                 = false;
    bool cutscene_or_fade               = false;
};

// The first reason that holds, or None. Hiding by itself does not block
// (the game limits the camera there itself). Pure.
TurnBlock turn_block(const TurnBlockInput& in) noexcept;

// A change of the block, for its debug line: the reason, and the
// interaction's kind when that is the reason.
struct TurnBlockChange {
    TurnBlock block                               = TurnBlock::None;
    input::Interaction kind                       = input::Interaction::None;
    bool operator==(const TurnBlockChange&) const = default;
};

// INF: the turning in use changed. Game thread.
void log_turning(diag::ports::Log& log, const TurningInUse& in_use) noexcept;
// DBG (the caller checks DebugOptions): turning blocked or unblocked.
void log_turn_block(diag::ports::Log& log, const TurnBlockChange& change) noexcept;

} // namespace omvr::core::stereo
