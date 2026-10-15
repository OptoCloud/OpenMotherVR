#pragma once

#include "util/base/error.h"

#include <filesystem>
#include <memory>

namespace omvr::core::diag::ports {
class Log;
}
namespace omvr::core::vr {
class RuntimeFailure;
}
namespace omvr::platform::win32 {
class ArchiveFiles;
}

namespace omvr::platform::openvr {

// The OpenVR loader (openvr_api.dll) and the scene session it opens
// (03 §6.4). The four extern "C" entry points are resolved with
// GetProcAddress rather than through an import library: a static import of
// openvr_api.dll, which lives outside any default search path, would keep
// this proxy from loading on machines without SteamVR.
//
// Game thread (CreateDevice); generic_interface also on the render thread
// once init has returned.
class ApiLoader final {
public:
    ApiLoader(core::diag::ports::Log& log, core::vr::RuntimeFailure& failure) noexcept;
    // Frees what is still loaded; after shutdown() that is nothing.
    ~ApiLoader();
    ApiLoader(const ApiLoader&) = delete;
    ApiLoader& operator=(const ApiLoader&) = delete;

    // Brings up an OpenVR scene session. `runtime_root` is the directory
    // containing bin/win32/openvr_api.dll, as the runtime probe found it;
    // the embedded openvr_api.dll is deployed through `payloads` first.
    // True when the loader loaded, VR_InitInternal2 succeeded and IVRSystem
    // is served. False, logged and reported to the RuntimeFailure, when:
    //   - the loader is missing or cannot be loaded;
    //   - any of the four required exports is missing;
    //   - VR_InitInternal2 reports an error (no headset, runtime busy);
    //   - IVRSystem is not served.
    // The session stays open until shutdown().
    bool init(const std::filesystem::path& runtime_root, win32::ArchiveFiles& payloads) noexcept;

    // VR_ShutdownInternal and unload. Safe when init never succeeded, and
    // more than once.
    void shutdown() noexcept;

    // VR_GetGenericInterface after a successful init(), for example with
    // vr::IVRCompositor_Version. An error carries the EVRInitError the
    // runtime returned, so a start-up failure can report the runtime's own
    // code (03 §6.4).
    util::Result<void*> generic_interface(const char* version) noexcept;

private:
    struct State;

    bool load(const std::filesystem::path& dll_path) noexcept;
    bool resolve_entries() noexcept;
    void preload_vrclient(const std::filesystem::path& runtime_root) noexcept;
    bool check_system() noexcept;
    const char* error_text(int error) const noexcept;
    void unload() noexcept;

    core::diag::ports::Log&    log_;
    core::vr::RuntimeFailure&  failure_;
    std::unique_ptr<State>     state_;
};

} // namespace omvr::platform::openvr
