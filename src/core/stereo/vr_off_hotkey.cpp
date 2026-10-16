// OpenMotherVR-Provenance: clean
#include "core/stereo/vr_off_hotkey.h"

#include "core/stereo/tuning.h"

namespace omvr::core::stereo {

bool VrOffHotkey::update(bool chord_held, TimePoint now) noexcept {
    if (fired_) return false;
    if (!chord_held) {
        held_since_.reset();
        return false;
    }
    if (!held_since_) held_since_ = now;
    if (now - *held_since_ < kVrOffHold) return false;
    fired_ = true;
    return true;
}

} // namespace omvr::core::stereo
