// OpenMotherVR-Provenance: clean
#pragma once

#include <chrono>
#include <optional>

// The VR-off hotkey (22 §13): right Ctrl + right Shift + F10 held together
// for kVrOffHold turns VR off for the rest of the session. The hold keeps a
// brushed key from ending VR by accident. Pure; game thread.
namespace omvr::core::stereo {

class VrOffHotkey final {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    // Called on every stereo update with whether the whole chord is down.
    // True exactly once: on the first update at which the chord has been
    // held without a break for kVrOffHold. Releasing any key restarts the
    // hold. After it fired it never fires again (VR cannot come back in
    // this session, 03 §6.5).
    bool update(bool chord_held, TimePoint now) noexcept;

    // It has fired.
    bool fired() const noexcept { return fired_; }

private:
    std::optional<TimePoint> held_since_;
    bool fired_ = false;
};

} // namespace omvr::core::stereo
