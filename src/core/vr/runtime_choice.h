// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"

#include <filesystem>
#include <optional>

namespace omvr::core::vr {

// Result of probing the host for VR runtimes. `chosen` is the runtime the
// mod drives, after the precedence rules of choose_runtime (06 §1).
//
// `chosen` reflects what was DETECTED; the VR session itself starts later,
// in the game's Stereo::CreateDevice. A runtime can be detected and still
// fail to start then (no headset connected, runtime in an error state).
struct DetectResult {
    bool openvr_available       = false;
    bool openxr_available       = false;
    bool forced                 = false; // -vrforce<runtime>: no fallback
    config::VRRuntime preferred = config::VRRuntime::OpenVR;
    // Enumerators spelled as config::VRRuntime spells them. There is no
    // Oculus: Meta's PC runtime is driven through OpenXR (06 §1.1).
    enum class Chosen { None, OpenVR, OpenXR };
    Chosen chosen = Chosen::None;

    // The OpenVR runtime root: the directory containing
    // bin/win32/openvr_api.dll. Empty when OpenVR is not available.
    std::filesystem::path openvr_runtime_path;

    // The OpenXR runtime manifest usable from this 32-bit process. Empty
    // when OpenXR is not available.
    std::filesystem::path openxr_manifest;
};

// The runtime to drive, from the availability flags, the preference and
// whether it was forced (06 §1):
//
//   1. A -vrforcenone / -vrforceopenvr / -vrforceoculus / -vrforceopenxr
//      switch on the command line picks the runtime. A forced runtime that
//      is missing means no VR, with no fallback. -vrforceoculus selects
//      OpenXR.
//   2. Otherwise the player's Settings.VRRuntime preference, if that
//      runtime is available ("None" means no VR; "Oculus", which the
//      settings turn into OpenXR on load, is never available).
//   3. Otherwise whichever runtime is present.
//   4. If none is present, None: the mod stays loaded but inert.
DetectResult::Chosen choose_runtime(const DetectResult& r) noexcept;

const char* to_string(DetectResult::Chosen c) noexcept;

// The settings' runtime id of `c`; none for Chosen::None.
std::optional<config::VRRuntime> runtime_of(DetectResult::Chosen c) noexcept;

} // namespace omvr::core::vr
