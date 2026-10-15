#include "loader/hooks/audio_hooks.h"

#include "loader/context.h"

#include <windows.h>
#include <mmdeviceapi.h>

#include <type_traits>

namespace omvr::loader::hooks {

namespace {

// IMMDevice::Activate: the headset's device instead of the one the game
// opens. Called by COM on the game's audio threads.
HRESULT __stdcall hook_activate(IMMDevice* self, REFIID iid, DWORD clsctx, PROPVARIANT* params, void** out) noexcept {
    return context().headset_audio.redirect().activate(self, iid, clsctx, params, out);
}

static_assert(std::is_convertible_v<decltype(&hook_activate), platform::audio::EndpointRedirect::ActivateFn>);

} // namespace

platform::audio::EndpointRedirect::ActivateFn activate_replacement() noexcept { return &hook_activate; }

} // namespace omvr::loader::hooks
