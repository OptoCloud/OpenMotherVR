#pragma once

#include "core/diag/ports/log.h"
#include "core/ui/ports/dialogs.h"
#include "core/vr/ports/headset_audio.h"
#include "game/addresses.h"
#include "platform/audio/endpoint_redirect.h"

#include <string>

namespace omvr::loader {

// Play the game's audio on the headset's own output device. `endpoint_id` is
// an MMDevice endpoint ID, as the runtime reports it for its headphones
// (XR_OCULUS_audio_device_guid; ovr_GetAudioDeviceOutGuidStr in the
// original). Hooks IMMDevice::Activate so whichever device the game opens,
// the headset's is activated instead, then has the game reopen its output.
// Takes effect once; later calls are ignored. The thread that starts the
// runtime.
// From: FUN_1001d630 (install), FUN_10001d60 (hook), FUN_100037b0 (caller).
class HeadsetAudio final : public core::vr::ports::HeadsetAudio {
public:
    HeadsetAudio(core::ui::ports::Dialogs& dialogs, core::diag::ports::Log& log,
                 const game::AddressMap& addresses) noexcept
        : dialogs_(dialogs), log_(log), addresses_(addresses) {}

    void redirect_to(std::wstring endpoint_id) noexcept override;

    // The redirect the Activate thunk forwards to.
    platform::audio::EndpointRedirect& redirect() noexcept { return redirect_; }

private:
    core::ui::ports::Dialogs& dialogs_;
    core::diag::ports::Log& log_;
    const game::AddressMap& addresses_;
    platform::audio::EndpointRedirect redirect_;
};

} // namespace omvr::loader
