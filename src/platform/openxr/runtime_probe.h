#pragma once

#include "core/vr/ports/runtime_probe.h"

#include <filesystem>

namespace omvr::platform::openxr {

class Session;

// Finds an OpenXR runtime this 32-bit process can load (Session::probe);
// `location` is its manifest.
class RuntimeProbe final : public core::vr::ports::RuntimeProbe {
public:
    explicit RuntimeProbe(Session& session) noexcept : session_(session) {}
    core::vr::ports::ProbeResult probe() override;

private:
    Session& session_;
};

// The loader ignores XR_RUNTIME_JSON in an elevated process; a loader
// property (XR_EXT_loader_init_properties) is honoured there too. Must run
// before the first other loader call.
void point_loader_at(const std::filesystem::path& manifest) noexcept;

// Hands the chosen manifest to the loader through the environment (this
// module's CRT copy and the process), only once OpenXR is the runtime that
// starts: a probe for a runtime the player did not choose must not leave the
// variable to the game's child processes (01 §5.2). The elevated line goes
// with it, since only then is the loader property the one that counts.
void set_runtime_environment(const std::filesystem::path& manifest) noexcept;

} // namespace omvr::platform::openxr
