#pragma once

#include "core/input/ports/controls_sink.h"
#include "core/ui/ports/dialogs.h"
#include "core/vr/ports/vr_runtime.h"

#include <memory>

namespace omvr::platform::win32 {
class ArchiveFiles;
}

namespace omvr::platform::openvr {

// What the OpenVR runtime uses from the rest of the mod.
struct OpenVrRuntimeDeps {
    win32::ArchiveFiles&                 payloads;   // deploys the embedded openvr_api.dll
    core::input::ports::ControlsSink&    controls;
    core::ui::ports::Dialogs&            dialogs;
};

// MotherVR::OpenVR, the OpenVR runtime behind the stereo device.
std::unique_ptr<core::vr::ports::VrRuntime> make_openvr_runtime(const OpenVrRuntimeDeps& deps);

} // namespace omvr::platform::openvr
