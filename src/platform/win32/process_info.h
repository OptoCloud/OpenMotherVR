// OpenMotherVR-Provenance: clean
#pragma once

#include <cstdint>
#include <string>

namespace omvr::platform::win32 {

// The Windows build number. RtlGetVersion, not GetVersionEx: the latter
// answers what the game's manifest claims to support, not the real build.
unsigned long windows_build() noexcept;

// Starting the game can run AI.exe twice, and both processes write the
// log: the one Steam started (it sets SteamGameId / SteamAppId) is "Game",
// the other the "Launcher" that asks Steam to relaunch it (spec 11 §1.8).
const char* process_kind() noexcept;

// Name of a process's executable, for the log; empty if unknown.
std::string process_name(std::uint32_t pid);

// This process's id.
std::uint32_t current_process_id() noexcept;

// The calling thread's id. Never 0.
std::uint32_t current_thread_id() noexcept;

// Whether this process runs elevated (as administrator).
bool process_elevated() noexcept;

} // namespace omvr::platform::win32
