#include "loader/headset_audio.h"

#include "core/ui/compat_messages.h"
#include "game/audio/audio_reopen.h"
#include "loader/hooks/audio_hooks.h"

#include <utility>

namespace omvr::loader {

void HeadsetAudio::redirect_to(std::wstring endpoint_id) noexcept {
    if (endpoint_id.empty() || redirect_.installed()) return;
    if (const long hr = redirect_.install(std::move(endpoint_id), hooks::activate_replacement(), log_);
        hr < 0) {
        core::diag::ports::logf(log_,
                                "audio: game output stays on the default device: redirect failed hr=0x{:08X}",
                                static_cast<unsigned>(hr));
        dialogs_.show_now(core::ui::audio_failed(static_cast<unsigned>(hr)),
                          core::ui::ports::DialogKind::Error, "audio failed");
        return;
    }
    log_.line("audio: game output redirected to the headset");

    // The game reopens its audio output, now through the hook.
    if (!game::audio::reopen_output(addresses_)) {
        log_.line(
            "audio: output reopen not available (entry bytes differ); "
            "switch happens at the game's next reopen");
    }
}

} // namespace omvr::loader
