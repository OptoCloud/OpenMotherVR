#pragma once

#include "core/config/ports/launch_switches.h"
#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/vr/ports/runtime_probe.h"
#include "core/vr/runtime_choice.h"

#include <mutex>

namespace omvr::core::vr {

// Probes for installed VR runtimes and chooses the one the mod drives.
// Discovery reads each runtime's published install location without loading
// any of its DLLs (06 §1):
//
//   OpenVR: %LOCALAPPDATA%\openvr\openvrpaths.vrpath (JSON written by
//           SteamVR's vrpathreg.exe), the first entry of "runtime"[], with
//           <runtime>\bin\win32\openvr_api.dll present (03 §6.4).
//   OpenXR: the active runtime from the Khronos registry key, or its 32-bit
//           sibling manifest (09 §1).
//   Oculus: not probed. LibOVR is not driven directly; Oculus/Meta headsets
//           run through OpenXR (-vrforceoculus selects it).
//
// No runtime is started here: the chosen one starts in the game's
// Stereo::CreateDevice.
class RuntimeDetector final {
public:
    // `settings` is the live settings object: the preference is read when
    // detection runs.
    RuntimeDetector(diag::ports::Log& log, const config::Settings& settings,
                    const config::ports::LaunchSwitches& switches, ports::RuntimeProbe& openvr,
                    ports::RuntimeProbe& openxr) noexcept
        : log_(log), settings_(settings), switches_(switches), openvr_(openvr), openxr_(openxr) {}

    // Probe and decide. Reads the settings and the command line, and logs
    // each step so the log shows why the runtime was picked. The result is
    // cached; last_result() returns it without probing again.
    DetectResult detect() noexcept;

    // The result of the most recent detect(); runs detect() on first use,
    // so detection happens when the stereo device is created rather than in
    // DllMain. Any thread.
    const DetectResult& last_result() noexcept;

private:
    diag::ports::Log&       log_;
    const config::Settings& settings_;
    const config::ports::LaunchSwitches& switches_;
    ports::RuntimeProbe&    openvr_;
    ports::RuntimeProbe&    openxr_;
    DetectResult            last_{};
    std::once_flag          detected_;
};

} // namespace omvr::core::vr
