#include "game_events.h"

#include "config/config.h"
#include "diag/log.h"
#include "hook/detour.h"

#include <array>
#include <functional>
#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

namespace mothervr::game {

namespace {

std::atomic<bool>        g_in_interaction{false};
std::atomic<Interaction> g_interaction{Interaction::None};
std::atomic<bool>        g_hiding{false};

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

void end_interaction() noexcept {
    g_in_interaction.store(false, std::memory_order_relaxed);
    g_interaction.store(Interaction::None, std::memory_order_relaxed);
}

// Returns true when `text` changed the state.
bool on_event(std::string_view text) noexcept {
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

// Dev debug: each distinct string once, and every state change, so new
// event names can be found by playing.
void log_event(std::string_view text, bool changed) noexcept {
    static std::mutex            mutex;
    static std::set<std::string, std::less<>> seen;   // find() without a copy
    try {
        const std::lock_guard lock{mutex};
        const bool first = seen.find(text) == seen.end();
        if (first) seen.emplace(text);
        if (!first && !changed) return;
        diag::logf("[events] \"{}\"{}: interaction {} kind {} hiding {}", text, first ? " (new)" : "",
                   in_interaction(), static_cast<int>(interaction()), hiding());
    } catch (...) {
    }
}

using TapFn = void* (__fastcall*)(void* self, void* edx, const char* text);
TapFn g_original = nullptr;

// __thiscall(this, const char*), callee pops the argument: __fastcall with
// the unused edx matches it.
void* __fastcall tap(void* self, void* edx, const char* text) noexcept {
    if (text) {
        const std::string_view s{text};
        const bool changed = on_event(s);
        if (config::current().debug.debug) log_event(s, changed);
    }
    return g_original(self, edx, text);
}

} // namespace

bool in_interaction() noexcept { return g_in_interaction.load(std::memory_order_relaxed); }
Interaction interaction() noexcept { return g_interaction.load(std::memory_order_relaxed); }
bool hiding() noexcept { return g_hiding.load(std::memory_order_relaxed); }

bool install_event_tap(std::uint8_t* at) noexcept {
    static constexpr std::uint8_t kNoOp[] = {0x8B, 0xC1, 0xC2, 0x04, 0x00};   // mov eax, ecx; ret 4
    return hook::detour(at, &tap, kNoOp, g_original, "game event tap");
}

} // namespace mothervr::game
