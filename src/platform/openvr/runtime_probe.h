// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/vr/ports/runtime_probe.h"

namespace omvr::platform::openvr {

// Finds SteamVR without loading it (03 §6.4): %LOCALAPPDATA%\openvr\
// openvrpaths.vrpath (JSON written by SteamVR's vrpathreg.exe), the first
// entry of "runtime"[], with <runtime>\bin\win32\openvr_api.dll present.
// `location` is the runtime root. Logs what it found (11 §2.3).
class RuntimeProbe final : public core::vr::ports::RuntimeProbe {
public:
    explicit RuntimeProbe(core::diag::ports::Log& log) noexcept : log_(log) {}

    core::vr::ports::ProbeResult probe() override;

private:
    core::diag::ports::Log& log_;
};

} // namespace omvr::platform::openvr
