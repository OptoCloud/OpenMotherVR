#pragma once

#include "core/vr/ports/runtime_probe.h"

namespace omvr::platform::openxr {

class Session;

// Finds an OpenXR runtime this 32-bit process can load (Session::probe,
// 09 §1); `location` is its manifest.
class RuntimeProbe final : public core::vr::ports::RuntimeProbe {
public:
    explicit RuntimeProbe(Session& session) noexcept : session_(session) {}
    core::vr::ports::ProbeResult probe() override;

private:
    Session& session_;
};

} // namespace omvr::platform::openxr
