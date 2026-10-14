#pragma once

#include <string>

namespace mothervr::audio {

// Play the game's audio on the headset's own output device. `endpoint_id` is
// an MMDevice endpoint ID, as the runtime reports it for its headphones
// (XR_OCULUS_audio_device_guid; ovr_GetAudioDeviceOutGuidStr in the
// original). Hooks IMMDevice::Activate so whichever device the game opens,
// the headset's is activated instead, then has the game reopen its output.
// Empty: nothing to do. Takes effect once; later calls are ignored.
// From: FUN_1001d630 (install), FUN_10001d60 (hook), FUN_100037b0 (caller).
void redirect_to(std::wstring endpoint_id) noexcept;

} // namespace mothervr::audio
