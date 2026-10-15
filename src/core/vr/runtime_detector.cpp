// Probe for installed VR runtimes and choose which one the mod should
// drive. Choice precedence is documented in runtime_choice.h.

#include "core/vr/runtime_detector.h"

#include "core/config/launch_options.h"
#include "core/config/settings_store.h"
#include "core/config/settings_table.h"
#include "core/diag/logger.h"

#include <atomic>

namespace omvr::core::vr {

namespace {

std::atomic<RuntimeDetector*> g_process_detector{nullptr};

} // namespace

detect_result RuntimeDetector::detect() noexcept {
    detect_result r{};

    // OpenVR via openvrpaths.vrpath (the probe logs what it found).
    const ports::ProbeResult openvr = openvr_.probe();
    r.openvr_available    = openvr.available;
    r.openvr_runtime_path = openvr.location;

    // Oculus: served by OpenXR, see runtime_detector.h.
    r.oculus_available = false;

    const ports::ProbeResult openxr = openxr_.probe();
    r.openxr_manifest  = openxr.location;
    r.openxr_available = !r.openxr_manifest.empty();

    const auto forced = config::forced_runtime();
    r.forced    = forced.has_value();
    r.preferred = forced.value_or(config::current().vr_runtime);

    diag::logf("vr probe: openvr={} oculus={} openxr={} preferred={}{}",
               r.openvr_available, r.oculus_available, r.openxr_available,
               config::to_string(r.preferred), r.forced ? " (forced)" : "");

    if (r.preferred == config::VRRuntime::None) {
        r.chosen = detect_result::Chosen::None; // user picked "Disable VR"
    } else if (r.preferred == config::VRRuntime::OpenVR && r.openvr_available) {
        r.chosen = detect_result::Chosen::OpenVR;
    } else if (r.preferred == config::VRRuntime::Oculus && r.oculus_available) {
        r.chosen = detect_result::Chosen::Oculus;
    } else if (r.preferred == config::VRRuntime::OpenXR && r.openxr_available) {
        r.chosen = detect_result::Chosen::OpenXR;
    } else if (r.forced) {
        r.chosen = detect_result::Chosen::None; // forced runtime missing
    } else if (r.openvr_available) {
        r.chosen = detect_result::Chosen::OpenVR;
    } else if (r.oculus_available) {
        r.chosen = detect_result::Chosen::Oculus;
    } else if (r.openxr_available) {
        r.chosen = detect_result::Chosen::OpenXR;
    } else {
        r.chosen = detect_result::Chosen::None;
    }

    diag::logf("vr probe: chosen={}", to_string(r.chosen));
    g_last = r;
    return r;
}

const detect_result& RuntimeDetector::last_result() noexcept {
    std::call_once(detected_, [this] { detect(); });  // once, thread-safe
    return g_last;
}

void set_process_detector(RuntimeDetector* detector) noexcept {
    g_process_detector.store(detector, std::memory_order_release);
}

const detect_result& last_result() noexcept {
    return g_process_detector.load(std::memory_order_acquire)->last_result();
}

} // namespace omvr::core::vr
