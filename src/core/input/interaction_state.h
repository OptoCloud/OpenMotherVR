#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

// What the game's scripts are doing, from the debug strings they pass to a
// retail no-op: AI.exe 0xe1bcc0 (`mov eax, ecx; ret 4`, a __thiscall taking
// a const char*, 683 call sites). Interactions (terminals, keypads, levers,
// hacking, cutting, suit changes, ladders) and hiding announce their start
// and end there. Later features read this state: d-pad and lever emulation
// and lean/hold-breath while hiding, turn blocking during interactions.
//
// Not in the original.
namespace omvr::core::input {

enum class Interaction : std::uint8_t {
    None, Generic, SuitChange, Keypad, Hacking, Cutting, Terminal, Ladder, Rewire,
};

class InteractionState {
public:
    // Safe from any thread.
    bool        in_interaction() const noexcept;  // between start_interaction and its end
    Interaction interaction() const noexcept;     // the specific kind, when announced
    bool        hiding() const noexcept;          // in a locker or under a table

    // One string the scripts passed. Returns true when it changed the state.
    bool on_event(std::string_view text) noexcept;

    // Dev debug: each distinct string once, and every state change, so new
    // event names can be found by playing.
    void log_event(std::string_view text, bool changed) noexcept;

private:
    void end_interaction() noexcept;

    std::atomic<bool>        g_in_interaction{false};
    std::atomic<Interaction> g_interaction{Interaction::None};
    std::atomic<bool>        g_hiding{false};
    std::mutex                         mutex_;
    std::set<std::string, std::less<>> seen_;   // find() without a copy
};

} // namespace omvr::core::input
