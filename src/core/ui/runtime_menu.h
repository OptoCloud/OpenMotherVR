#pragma once

#include "core/config/settings.h"

// The options page's runtime entries.
namespace omvr::core::ui {

// The menu's runtime entries: 0 none, 1 the original's "Oculus" slot, now
// "OpenXR" (the UI patch relabels it; the original's LibOVR backend is not
// reconstructed, and Meta's runtime is an OpenXR runtime), 2 OpenVR.
int ui_runtime_value(config::VRRuntime r) noexcept;

// The runtime this run drives: None when the chosen one failed to start, the
// fallback when the stored one was not available. The menu then offers the
// stored runtime (a retry) and the other one (the way back), where showing
// the stored choice would hide that VR is not running. A -vrforce* switch
// overrides the stored runtime for this run (06 section 4.3), so a stored
// None with -vrforceopenvr shows OpenVR.
config::VRRuntime running_runtime(const config::Settings& cfg) noexcept;

// The runtime a menu entry stands for.
config::VRRuntime runtime_from_ui(int v) noexcept;

} // namespace omvr::core::ui
