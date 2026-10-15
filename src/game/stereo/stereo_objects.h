#pragma once

#include "game/stereo_extension.h"

#include <array>
#include <atomic>

// The stereo manager and device the game is given in place of its own:
// their vtables, initial values and the atomic access to the fields other
// threads read.
namespace omvr::game::stereo {

// The device state is read on the game's job threads (the cull hooks) and
// the render thread while the main thread changes it, and a late detach
// (slot 3, render thread) clears it: writes, and reads off the main thread,
// go through atomic_ref. Relaxed: a value one frame stale is harmless. The
// layout is the game's and stays as it is.
inline DeviceState load_state(StereoInterface& si) noexcept {
    return std::atomic_ref(si.state).load(std::memory_order_relaxed);
}
inline void store_state(StereoInterface& si, DeviceState s) noexcept {
    std::atomic_ref(si.state).store(s, std::memory_order_relaxed);
}

// As load_state / store_state, for the attached device.
inline StereoInterface* load_device(StereoManager& m) noexcept {
    return std::atomic_ref(m.device).load(std::memory_order_relaxed);
}
inline void store_device(StereoManager& m, StereoInterface* device) noexcept {
    std::atomic_ref(m.device).store(device, std::memory_order_relaxed);
}

// The device's 22 vtable slots and the manager's 3. Slots 12 to 14 of the
// device are the game's own and are filled in from the address table.
using DeviceSlots  = std::array<void*, 22>;
using ManagerSlots = std::array<void*, 3>;

// Builds both vtables (four device slots point into AI.exe) and the
// initial field values of `m`. Call once from the hook installer.
// From: FUN_10003510 + FUN_10003640.
void initialize(StereoManager& m, const DeviceSlots& device, const ManagerSlots& manager) noexcept;

} // namespace omvr::game::stereo
