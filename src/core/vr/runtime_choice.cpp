// OpenMotherVR-Provenance: clean
#include "core/vr/runtime_choice.h"

namespace omvr::core::vr {

namespace {

using Chosen = DetectResult::Chosen;
using config::VRRuntime;

// Rule 2: the preferred runtime is installed.
bool preferred_available(const DetectResult& r) noexcept {
    switch (r.preferred) {
        case VRRuntime::OpenVR: return r.openvr_available;
        case VRRuntime::Oculus: return r.oculus_available;
        case VRRuntime::OpenXR: return r.openxr_available;
        case VRRuntime::None: return false;
    }
    return false;
}

Chosen chosen_for(VRRuntime runtime) noexcept {
    switch (runtime) {
        case VRRuntime::OpenVR: return Chosen::OpenVR;
        case VRRuntime::Oculus: return Chosen::Oculus;
        case VRRuntime::OpenXR: return Chosen::OpenXR;
        case VRRuntime::None: return Chosen::None;
    }
    return Chosen::None;
}

// Rule 3: whichever runtime is present, in this order.
Chosen first_available(const DetectResult& r) noexcept {
    if (r.openvr_available) return Chosen::OpenVR;
    if (r.oculus_available) return Chosen::Oculus;
    if (r.openxr_available) return Chosen::OpenXR;
    return Chosen::None;
}

} // namespace

DetectResult::Chosen choose_runtime(const DetectResult& r) noexcept {
    if (r.preferred == VRRuntime::None) return Chosen::None; // the player picked "Disable VR"
    if (preferred_available(r)) return chosen_for(r.preferred);
    if (r.forced) return Chosen::None; // a forced runtime is missing
    return first_available(r);
}

const char* to_string(DetectResult::Chosen c) noexcept {
    switch (c) {
        case Chosen::OpenVR: return "OpenVR";
        case Chosen::Oculus: return "Oculus";
        case Chosen::OpenXR: return "OpenXR";
        case Chosen::None: return "None";
    }
    return "None";
}

} // namespace omvr::core::vr
