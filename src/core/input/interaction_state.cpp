#include "core/input/interaction_state.h"

#include "core/diag/logger.h"

#include <array>
#include <functional>

namespace omvr::core::input {

namespace {

// Interaction kinds announced as "<prefix>_start" / "<prefix>_finish".
struct Kind {
    std::string_view prefix;
    Interaction      kind;
};
constexpr std::array kKinds = {
    Kind{"suit_change_interaction", Interaction::SuitChange},
    Kind{"keypad_interaction",      Interaction::Keypad},
    Kind{"hacking_interaction",     Interaction::Hacking},
    Kind{"cutting_panel",           Interaction::Cutting},
    Kind{"terminal_interaction",    Interaction::Terminal},
    Kind{"ladder_interaction",      Interaction::Ladder},
    Kind{"rewire_interaction",      Interaction::Rewire},
};

// The strings that end any interaction.
constexpr std::array<std::string_view, 4> kEnds = {
    "stop_interaction", "ladder_interaction_finish", "hiding_visibility_finish",
    "suit_change_interaction_finish",
};

} // namespace

void InteractionState::end_interaction() noexcept {
    g_in_interaction.store(false, std::memory_order_relaxed);
    g_interaction.store(Interaction::None, std::memory_order_relaxed);
}

bool InteractionState::on_event(std::string_view text) noexcept {
    // Most traffic is per-frame engine chatter ("CATHODE:...", "on_update"):
    // none of the strings below starts with 'C' or "on_".
    if (text.empty() || text[0] == 'C' || text.starts_with("on_")) return false;
    if (text == "start_interaction") {
        g_in_interaction.store(true, std::memory_order_relaxed);
        if (g_interaction.load(std::memory_order_relaxed) == Interaction::None)
            g_interaction.store(Interaction::Generic, std::memory_order_relaxed);
        return true;
    }
    if (text == "hiding_visibility_start") {
        g_hiding.store(true, std::memory_order_relaxed);
        return true;
    }
    bool changed = false;
    if (text == "hiding_visibility_finish") {
        g_hiding.store(false, std::memory_order_relaxed);
        changed = true;
    }
    for (const auto end : kEnds) {
        if (text == end) {
            end_interaction();
            return true;
        }
    }
    for (const auto& k : kKinds) {
        if (!text.starts_with(k.prefix)) continue;
        const auto suffix = text.substr(k.prefix.size());
        if (suffix == "_start") {
            g_in_interaction.store(true, std::memory_order_relaxed);
            g_interaction.store(k.kind, std::memory_order_relaxed);
            return true;
        }
        if (suffix == "_finish") {
            Interaction expected = k.kind;
            g_interaction.compare_exchange_strong(expected, Interaction::Generic, std::memory_order_relaxed);
            return true;
        }
    }
    return changed;
}

void InteractionState::log_event(std::string_view text, bool changed) noexcept {
    try {
        const std::lock_guard lock{mutex_};
        const bool first = seen_.find(text) == seen_.end();
        if (first) seen_.emplace(text);
        if (!first && !changed) return;
        diag::logf("[events] \"{}\"{}: interaction {} kind {} hiding {}", text, first ? " (new)" : "",
                   in_interaction(), static_cast<int>(interaction()), hiding());
    } catch (...) {
    }
}

bool InteractionState::in_interaction() const noexcept { return g_in_interaction.load(std::memory_order_relaxed); }
Interaction InteractionState::interaction() const noexcept { return g_interaction.load(std::memory_order_relaxed); }
bool InteractionState::hiding() const noexcept { return g_hiding.load(std::memory_order_relaxed); }

} // namespace omvr::core::input
