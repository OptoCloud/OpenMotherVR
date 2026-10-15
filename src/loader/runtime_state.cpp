// OpenMotherVR-Provenance: clean
#include "loader/runtime_state.h"

namespace omvr::loader {

std::optional<core::config::VRRuntime> RuntimeStateView::forced() const noexcept {
    return launch_.forced_runtime();
}

bool RuntimeStateView::failed() const noexcept { return stereo_.runtime_failed(); }

core::vr::DetectResult::Chosen RuntimeStateView::chosen() noexcept { return detector_.last_result().chosen; }

bool RuntimeStateView::session_active() const noexcept { return stereo_.vr_session_active(); }

} // namespace omvr::loader
