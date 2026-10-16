// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

// What the game's scripts are doing, from the debug strings they pass to a
// retail no-op (05 §12, 11 §5). Interactions (terminals, keypads, levers,
// hacking, cutting, suit changes, ladders) and hiding announce their start
// and end there. Later features read this state: d-pad and lever emulation
// and lean/hold-breath while hiding, turn blocking during interactions.
namespace omvr::core::input {

enum class Interaction : std::uint8_t {
    None,
    Generic,
    SuitChange,
    Keypad,
    Hacking,
    Cutting,
    Terminal,
    Ladder,
    Rewire,
};

// The tracked state of 11 §5.3. Each value is one atomic, so any thread
// reads it without tearing; relaxed ordering is enough because the values
// are independent flags no other data hangs on.
class InteractionState final {
public:
    explicit InteractionState(diag::ports::Log& log) noexcept : log_(log) {}

    // Safe from any thread.
    [[nodiscard]] bool in_interaction() const noexcept;     // between start_interaction and its end
    [[nodiscard]] Interaction interaction() const noexcept; // the specific kind, when announced
    [[nodiscard]] bool hiding() const noexcept;             // in a locker or under a table
    // The hiding place's breathing game is in progress (spec 22 §9.1).
    [[nodiscard]] bool breathing_game() const noexcept;

    // One string the scripts passed. Returns true when it changed the state.
    // Game thread.
    bool on_event(std::string_view text) noexcept;

    // Dev debug (11 §4.3): each distinct string once, and every state
    // change, so new event names can be found by playing. Game thread.
    void log_event(std::string_view text, bool changed) noexcept;

    // A loading screen opened: the breathing game ends with the level
    // (spec 22 §9.1). Game thread.
    void on_loading_screen() noexcept;

private:
    void end_interaction() noexcept;
    bool on_prefixed_event(std::string_view text) noexcept;

    diag::ports::Log& log_;
    std::atomic<bool> in_interaction_{false};
    std::atomic<Interaction> interaction_{Interaction::None};
    std::atomic<bool> hiding_{false};
    std::atomic<bool> breathing_game_{false};
    std::mutex mutex_;                        // guards seen_; never held across a game call
    std::set<std::string, std::less<>> seen_; // find() without a copy
};

} // namespace omvr::core::input
