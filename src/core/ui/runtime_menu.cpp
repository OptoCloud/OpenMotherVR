#include "core/ui/runtime_menu.h"

#include "core/config/launch_options.h"
#include "core/stereo/stereo_device.h"
#include "core/vr/runtime_detector.h"

namespace omvr::core::ui {

namespace {

namespace runtime {
using config::forced_runtime;
using vr::detect_result;
using vr::last_result;
} // namespace runtime

namespace stereo {
using core::stereo::runtime_failed;
} // namespace stereo

} // namespace

int ui_runtime_value(config::VRRuntime r) noexcept {
    switch (r) {
        case config::VRRuntime::OpenXR:
        case config::VRRuntime::Oculus: return 1;
        case config::VRRuntime::OpenVR: return 2;
        default:                        return 0;
    }
}

config::VRRuntime running_runtime(const config::Settings& cfg) noexcept {
    const auto requested = runtime::forced_runtime().value_or(cfg.vr_runtime);
    if (requested == config::VRRuntime::None || stereo::runtime_failed()) return config::VRRuntime::None;
    switch (runtime::last_result().chosen) {
        case runtime::detect_result::Chosen::OpenVR: return config::VRRuntime::OpenVR;
        case runtime::detect_result::Chosen::OpenXR: return config::VRRuntime::OpenXR;
        default:                                     return config::VRRuntime::None;
    }
}

config::VRRuntime runtime_from_ui(int v) noexcept {
    return v == 1 ? config::VRRuntime::OpenXR
         : v == 2 ? config::VRRuntime::OpenVR
                  : config::VRRuntime::None;
}

} // namespace omvr::core::ui
