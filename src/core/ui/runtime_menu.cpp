#include "core/ui/runtime_menu.h"

#include <utility>

namespace omvr::core::ui {

RuntimeEntry ui_runtime_value(config::VRRuntime r) noexcept {
    switch (r) {
        case config::VRRuntime::OpenXR:
        case config::VRRuntime::Oculus: return RuntimeEntry::OpenXr;
        case config::VRRuntime::OpenVR: return RuntimeEntry::OpenVr;
        default: return RuntimeEntry::Off;
    }
}

config::VRRuntime running_runtime(const config::Settings& cfg, ports::RuntimeState& state) noexcept {
    const auto requested = state.forced().value_or(cfg.vr_runtime);
    if (requested == config::VRRuntime::None || state.failed()) return config::VRRuntime::None;
    switch (state.chosen()) {
        case vr::DetectResult::Chosen::OpenVR: return config::VRRuntime::OpenVR;
        case vr::DetectResult::Chosen::OpenXR: return config::VRRuntime::OpenXR;
        default: return config::VRRuntime::None;
    }
}

std::optional<config::VRRuntime> runtime_from_ui(int entry) noexcept {
    switch (static_cast<RuntimeEntry>(entry)) {
        case RuntimeEntry::Off: return config::VRRuntime::None;
        case RuntimeEntry::OpenXr: return config::VRRuntime::OpenXR;
        case RuntimeEntry::OpenVr: return config::VRRuntime::OpenVR;
    }
    return std::nullopt;
}

} // namespace omvr::core::ui
