// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/vr/runtime_choice.h"

#include <optional>

namespace omvr::core::ui::ports {

// What the options page needs to know about the VR runtime of this run,
// owned by the stereo device and the runtime detector. Implemented by the
// loader over them. Game thread.
class RuntimeState {
public:
    // The runtime a -vrforce* switch picks for this run, if any.
    virtual std::optional<config::VRRuntime> forced() const noexcept = 0;
    // The chosen runtime failed to start; VR is off this run.
    virtual bool failed() const noexcept = 0;
    // The runtime the detector chose. Runs the detection on first use, so
    // call it only when the answer is needed.
    virtual vr::DetectResult::Chosen chosen() noexcept = 0;
    // A runtime object exists and its session is live.
    virtual bool session_active() const noexcept = 0;

protected:
    ~RuntimeState() = default;
};

} // namespace omvr::core::ui::ports
