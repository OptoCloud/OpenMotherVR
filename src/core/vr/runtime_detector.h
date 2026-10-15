#pragma once

#include "core/vr/ports/runtime_probe.h"
#include "core/vr/runtime_choice.h"

#include <mutex>

namespace omvr::core::vr {

// Probes for installed VR runtimes and chooses the one the mod drives.
// Discovery reads each runtime's published install location without loading
// any of its DLLs:
//
//   OpenVR: %LOCALAPPDATA%\openvr\openvrpaths.vrpath (JSON written by
//           SteamVR's vrpathreg.exe), the first entry of "runtime"[], with
//           <runtime>\bin\win32\openvr_api.dll present.
//   OpenXR: the active runtime from the Khronos registry key, or its 32-bit
//           sibling manifest.
//   Oculus: not probed. LibOVR is not driven directly; Oculus/Meta headsets
//           run through OpenXR (-vrforceoculus selects it).
//
// No runtime is started here: the chosen one starts in the game's
// Stereo::CreateDevice.
class RuntimeDetector {
public:
    RuntimeDetector(ports::RuntimeProbe& openvr, ports::RuntimeProbe& openxr) noexcept
        : openvr_(openvr), openxr_(openxr) {}

    // Probe + decide. Reads the settings and the command line. Logs each step
    // so we can see exactly why the chosen runtime was picked. The result is
    // cached; call last_result() to retrieve it without re-probing.
    detect_result detect() noexcept;

    // The detect_result from the most recent detect() call; runs detect() on
    // first use, so detection happens when the stereo device is created rather
    // than in DllMain.
    const detect_result& last_result() noexcept;

private:
    ports::RuntimeProbe& openvr_;
    ports::RuntimeProbe& openxr_;
    detect_result        g_last{};
    std::once_flag       detected_;
};

// R: vr (A1) and its callers: take the RuntimeDetector at construction. Until
// then the loader registers its detector here at attach.
void set_process_detector(RuntimeDetector* detector) noexcept;
const detect_result& last_result() noexcept;

} // namespace omvr::core::vr
