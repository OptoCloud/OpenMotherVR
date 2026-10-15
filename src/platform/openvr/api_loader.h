#pragma once

#include <filesystem>

namespace omvr::platform::win32 {
class ArchiveFiles;
}

namespace omvr::platform::openvr {

// Try to bring up an OpenVR scene session. `runtime_root` is the directory
// containing bin/win32/openvr_api.dll, as the runtime probe found it; the
// embedded openvr_api.dll is deployed through `payloads` first. True when
// the runtime DLL loaded, VR_Init succeeded and IVRSystem is served. False
// (with a logged error and set_init_failure) on any failure path:
//   - runtime DLL missing or unloadable
//   - any of the four required exports missing
//   - VR_InitInternal2 reports an error (no HMD, runtime busy, etc.)
//   - IVRSystem interface query fails
//
// On success, the OpenVR session stays open until shutdown() is called.
bool init(const std::filesystem::path& runtime_root, win32::ArchiveFiles& payloads) noexcept;

// VR_ShutdownInternal + FreeLibrary. Safe to call when init never succeeded,
// and more than once.
void shutdown() noexcept;

// VR_GetGenericInterface after a successful init(), e.g. with
// vr::IVRCompositor_Version. Null if the runtime does not serve it; then
// `error`, when given, receives the EVRInitError it returned, so a start-up
// failure can report the runtime's own code (03 §6.4).
void* generic_interface(const char* version, int* error = nullptr) noexcept;

} // namespace omvr::platform::openvr
