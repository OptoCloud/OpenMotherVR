#pragma once

#include "core/vr/ports/runtime_probe.h"

namespace omvr::platform::openvr {

// Finds SteamVR without loading it: %LOCALAPPDATA%\openvr\openvrpaths.vrpath
// (JSON written by SteamVR's vrpathreg.exe), the first entry of "runtime"[],
// with <runtime>\bin\win32\openvr_api.dll present. `location` is the
// runtime root. Logs what it found.
class RuntimeProbe final : public core::vr::ports::RuntimeProbe {
public:
    core::vr::ports::ProbeResult probe() override;
};

} // namespace omvr::platform::openvr
