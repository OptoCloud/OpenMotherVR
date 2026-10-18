#pragma once

#include "core/diag/ports/clock.h"
#include "core/diag/ports/log.h"
#include "core/input/ports/controls_sink.h"
#include "core/ui/ports/dialogs.h"
#include "core/vr/ports/haptic_source.h"
#include "core/vr/ports/vr_runtime.h"

#include <filesystem>
#include <memory>

namespace omvr::core::vr {
class RuntimeFailure;
} // namespace omvr::core::vr
namespace omvr::platform::win32 {
class ArchiveFiles;
} // namespace omvr::platform::win32

namespace omvr::platform::openvr {

// What the OpenVR runtime uses from the rest of the mod.
struct OpenVrRuntimeDeps {
    core::diag::ports::Log& log;
    const core::diag::ports::Clock& clock; // the start-up wait (03 §6.4)
    core::vr::RuntimeFailure& failure;     // why the start failed, for the player
    win32::ArchiveFiles& payloads;         // deploys the embedded openvr_api.dll
    core::input::ports::ControlsSink& controls;
    core::ui::ports::Dialogs& dialogs;
    std::filesystem::path runtime_root; // as the runtime probe found it
    bool aim_poses = false;             // read the hands' aim poses; they are always declared (spec 21 §16)
    // The haptic output (spec 21 §14), asked each frame; null: no vibration
    // actions are declared and nothing vibrates.
    core::vr::ports::HapticSource* haptics = nullptr;
};

// MotherVR::OpenVR, the OpenVR runtime behind the stereo device.
std::unique_ptr<core::vr::ports::VrRuntime> make_openvr_runtime(const OpenVrRuntimeDeps& deps);

} // namespace omvr::platform::openvr
