// OpenMotherVR-Provenance: clean
// Probe for installed VR runtimes and choose which one the mod drives.
// The precedence is choose_runtime's (runtime_choice.h).

#include "core/vr/runtime_detector.h"

#include "core/config/settings_table.h"

#include <string_view>

namespace omvr::core::vr {

namespace {

// 11 §2.3: what was found, and what was chosen.
constexpr std::string_view kLogProbe  = "vr probe: openvr={} oculus={} openxr={} preferred={}{}";
constexpr std::string_view kLogChosen = "vr probe: chosen={}";

} // namespace

void RuntimeDetector::detect() noexcept {
    DetectResult r{};

    // OpenVR via openvrpaths.vrpath (the probe logs what it found).
    const ports::ProbeResult openvr = openvr_.probe();
    r.openvr_available              = openvr.available;
    r.openvr_runtime_path           = openvr.location;

    // Oculus: served by OpenXR, see runtime_detector.h.
    r.oculus_available = false;

    const ports::ProbeResult openxr = openxr_.probe();
    r.openxr_manifest               = openxr.location;
    r.openxr_available              = !r.openxr_manifest.empty();

    const auto forced = switches_.forced_runtime();
    r.forced          = forced.has_value();
    r.preferred       = forced.value_or(settings_.vr_runtime);

    diag::ports::logf(log_, kLogProbe, r.openvr_available, r.oculus_available, r.openxr_available,
                      config::to_string(r.preferred), r.forced ? " (forced)" : "");

    r.chosen = choose_runtime(r);
    diag::ports::logf(log_, kLogChosen, to_string(r.chosen));
    last_ = r;
}

const DetectResult& RuntimeDetector::last_result() noexcept {
    std::call_once(detected_, [this] { detect(); }); // once, thread-safe
    return last_;
}

} // namespace omvr::core::vr
