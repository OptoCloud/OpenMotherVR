#pragma once

#include "core/config/settings.h"

#include <filesystem>

namespace omvr::core::vr {

// Result of probing the host for VR runtimes. `chosen` is the backend the
// mod should drive, after applying the precedence rules:
//
//   1. A -vrforcenone / -vrforceopenvr / -vrforceoculus / -vrforceopenxr
//      switch on the command line picks the runtime, as in the original
//      (readme.txt). A forced runtime that is missing means no VR, with no
//      fallback. -vrforceoculus selects OpenXR.
//   2. Otherwise honor the user's `Settings.VRRuntime` preference from
//      mothervr.json if that backend is available ("None" means no VR).
//   3. Fall back to whichever backend is present.
//   4. If none is present, `chosen = None` and the mod stays loaded but
//      inert (no VR features active).
//
// `chosen` reflects what the loader DETECTED; the VR session itself starts
// later, in the game's Stereo::CreateDevice. A backend can be detected and still fail to
// initialize at session-create time (no headset connected, runtime in error
// state, etc.).
struct detect_result {
    bool                  openvr_available = false;
    bool                  oculus_available = false;
    bool                  openxr_available = false;
    bool                  forced           = false; // -vrforce<runtime>: no fallback
    config::VRRuntime     preferred        = config::VRRuntime::OpenVR;
    enum class Chosen { None, OpenVR, Oculus, OpenXR };
    Chosen                chosen           = Chosen::None;

    // Discovered runtime root. For OpenVR this is the directory containing
    // bin/win32/openvr_api.dll. Empty if the corresponding *_available flag
    // is false. Backend modules consume this directly so they don't have to
    // re-parse the runtime registry themselves.
    std::filesystem::path openvr_runtime_path;

    // OpenXR runtime manifest usable from this 32-bit process (see
    // openxr::probe). Empty if openxr_available is false.
    std::filesystem::path openxr_manifest;
};

const char* to_string(detect_result::Chosen c) noexcept;

} // namespace omvr::core::vr
