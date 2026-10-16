// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/thread_roles.h"

#include <cstddef>
#include <cstdint>
#include <span>

// The crash event of Controls.CrashSafeLog (22 §14): the mod's state at the
// moment of a crash, written beside v1's crash record (11 §6.1).
namespace omvr::core::diag {

// The states that help to place a crash, read without locks from atomics
// (or plain words the crashing process may have left half-written: this is
// a diagnostic, not a decision).
struct CrashState {
    ThreadRole thread         = ThreadRole::Other; // the crashing thread (11 §1.4)
    bool device_attached      = false;             // the game holds a stereo device (manager +0x04)
    std::int32_t device_state = 0;                 // 02 §7.1: 0 none, 1 ready, 2 running
    std::int32_t stereo_mode  = 0;                 // 05: 0 world, 1 screen (menus, videos)
    std::uint32_t interaction = 0;                 // core::input::Interaction, 0 none
    bool hiding               = false;             // in a locker or under a table
    std::uint32_t screen      = 0;                 // core::ui::Screen, 0 none (no menu open)
    bool cutscene             = false;
    bool vr_turned_off        = false; // the VR-off hotkey fired (22 §13)
};

// Renders the event's message (the logger adds the prefix) into `out`, cut
// to fit. Returns its length. No allocation: it runs inside the crash
// filter, in whatever state the process is.
std::size_t format_crash_state(std::span<char> out, const CrashState& s) noexcept;

} // namespace omvr::core::diag
