// OpenMotherVR-Provenance: clean
#include "core/input/interaction_state.h"

#include "game/constants.h"

#include <array>

namespace omvr::core::input {

namespace {

// Interaction kinds announced as "<prefix>_start" / "<prefix>_finish"
// (11 §5.3).
struct Kind {
    std::string_view prefix;
    Interaction kind;
};
constexpr std::array kKinds = {
    Kind{game::kEventSuitChange, Interaction::SuitChange}, Kind{game::kEventKeypad, Interaction::Keypad},
    Kind{game::kEventHacking, Interaction::Hacking},       Kind{game::kEventCutting, Interaction::Cutting},
    Kind{game::kEventTerminal, Interaction::Terminal},     Kind{game::kEventLadder, Interaction::Ladder},
    Kind{game::kEventRewire, Interaction::Rewire},
};

// The strings that end any interaction.
constexpr std::array kEnds = {
    game::kEventStopInteraction,
    game::kEventLadderFinish,
    game::kEventHidingFinish,
    game::kEventSuitChangeFinish,
};

constexpr std::string_view kEventLog = "[events] \"{}\"{}: interaction {} kind {} hiding {}";
constexpr std::string_view kNewTag   = " (new)";

// 11 §5.2: most traffic is per-frame engine chatter that no tracked string
// resembles; drop it before any matching.
bool is_chatter(std::string_view text) noexcept {
    return text.empty() || text[0] == game::kEventChatterInitial ||
           text.starts_with(game::kEventChatterPrefix);
}

bool is_end(std::string_view text) noexcept {
    for (const auto end : kEnds) {
        if (text == end) return true;
    }
    return false;
}

} // namespace

void InteractionState::end_interaction() noexcept {
    in_interaction_.store(false, std::memory_order_relaxed);
    interaction_.store(Interaction::None, std::memory_order_relaxed);
}

// "<prefix>_start" starts that kind; "<prefix>_finish" ends it back to
// generic if it was that kind. False for any other string.
bool InteractionState::on_prefixed_event(std::string_view text) noexcept {
    for (const auto& k : kKinds) {
        if (!text.starts_with(k.prefix)) continue;
        const auto suffix = text.substr(k.prefix.size());
        if (suffix == game::kEventStartSuffix) {
            in_interaction_.store(true, std::memory_order_relaxed);
            interaction_.store(k.kind, std::memory_order_relaxed);
            return true;
        }
        if (suffix == game::kEventFinishSuffix) {
            Interaction expected = k.kind;
            interaction_.compare_exchange_strong(expected, Interaction::Generic, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

bool InteractionState::on_event(std::string_view text) noexcept {
    if (is_chatter(text)) return false;
    if (text == game::kEventStartInteraction) {
        in_interaction_.store(true, std::memory_order_relaxed);
        if (interaction_.load(std::memory_order_relaxed) == Interaction::None)
            interaction_.store(Interaction::Generic, std::memory_order_relaxed);
        return true;
    }
    if (text == game::kEventHidingStart) {
        hiding_.store(true, std::memory_order_relaxed);
        return true;
    }
    bool changed = false;
    if (text == game::kEventHidingFinish) {
        hiding_.store(false, std::memory_order_relaxed);
        changed = true;
    }
    if (is_end(text)) {
        end_interaction();
        return true;
    }
    return on_prefixed_event(text) || changed;
}

void InteractionState::log_event(std::string_view text, bool changed) noexcept {
    // The set may allocate; a failure only costs this log line.
    try {
        const std::lock_guard lock{mutex_};
        const bool first = seen_.find(text) == seen_.end();
        if (first) seen_.emplace(text);
        if (!first && !changed) return;
        diag::ports::logf(log_, kEventLog, text, first ? kNewTag : std::string_view{}, in_interaction(),
                          static_cast<int>(interaction()), hiding());
    } catch (...) {}
}

bool InteractionState::in_interaction() const noexcept {
    return in_interaction_.load(std::memory_order_relaxed);
}
Interaction InteractionState::interaction() const noexcept {
    return interaction_.load(std::memory_order_relaxed);
}
bool InteractionState::hiding() const noexcept { return hiding_.load(std::memory_order_relaxed); }

} // namespace omvr::core::input
