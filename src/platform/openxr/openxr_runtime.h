#pragma once

#include "core/diag/ports/log.h"
#include "core/input/ports/controls_sink.h"
#include "core/ui/ports/game_signals.h"
#include "core/vr/ports/headset_audio.h"
#include "core/vr/ports/vr_runtime.h"

#include <memory>

namespace omvr::platform::openxr {

class Session;

// What the OpenXR runtime uses from the rest of the mod.
struct OpenXrRuntimeDeps {
    core::diag::ports::Log&           log;
    Session&                          session;
    core::input::ports::ControlsSink& controls;
    core::vr::ports::HeadsetAudio&    audio;
    core::ui::ports::GameSignals&     signals;   // the runtime's quit request
};

// The stereo device on any OpenXR runtime (Meta / Quest Link, SteamVR, WMR,
// Pico, Varjo...). Not in the original (09).
std::unique_ptr<core::vr::ports::VrRuntime> make_openxr_runtime(const OpenXrRuntimeDeps& deps);

} // namespace omvr::platform::openxr
