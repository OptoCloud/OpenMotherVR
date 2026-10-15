// OpenVR session bring-up. The four extern "C" entry points we use match the
// original mod's static imports from OPENVR_API.DLL:
//   VR_InitInternal2, VR_ShutdownInternal, VR_GetGenericInterface,
//   VR_GetVRInitErrorAsEnglishDescription
//
// We resolve them dynamically with LoadLibrary/GetProcAddress instead of
// linking openvr_api.lib: the import lib would add a static dependency on
// openvr_api.dll, which lives outside any default search path and would make
// our d3d11 proxy fail to load on machines without SteamVR. Loading at
// runtime, after vr_detect has located the runtime, side-steps that.
//
// The IVRSystem* returned by VR_GetGenericInterface is the C++ vtable form
// (openvr.h declares it as a class with virtuals; OpenVR's official SDK
// ships an MSVC-compatible vtable, so calling member functions on it from
// our MSVC build is well-defined).

#include "platform/openvr/api_loader.h"

#include "core/diag/logger.h" // R: vr (A1): log through an injected Log&
#include "core/vr/runtime_failure.h" // R: vr (A1): report the failure through the runtime's result
#include "platform/win32/dll_directory_scope.h"
#include "platform/win32/module.h"
#include "platform/win32/payload_files.h"
#include "platform/win32/proc.h"
#include "platform/win32/text.h"

#include <openvr.h>
#include <windows.h>

#include <cstdint>
#include <string>

namespace omvr::platform::openvr {

namespace {

namespace diag {
using core::diag::log_line;
using core::diag::logf;
using win32::module_directory;
using win32::utf8;
} // namespace diag
namespace util = win32;
using core::vr::set_init_failure;
using win32::DllDirectoryScope;

// R: vr (A1): the loader's modules into an owner (UniqueModule).
HMODULE g_dll      = nullptr;
HMODULE g_vrclient = nullptr;   // our extra reference from the pre-load

using PFN_VR_InitInternal2                     = std::uint32_t (__cdecl*)(vr::EVRInitError*, vr::EVRApplicationType, const char*);
using PFN_VR_ShutdownInternal                  = void          (__cdecl*)();
using PFN_VR_GetGenericInterface               = void*         (__cdecl*)(const char*, vr::EVRInitError*);
using PFN_VR_GetVRInitErrorAsEnglishDescription = const char*   (__cdecl*)(vr::EVRInitError);

PFN_VR_InitInternal2                      g_VR_InitInternal2                     = nullptr;
PFN_VR_ShutdownInternal                   g_VR_ShutdownInternal                  = nullptr;
PFN_VR_GetGenericInterface                g_VR_GetGenericInterface               = nullptr;
PFN_VR_GetVRInitErrorAsEnglishDescription g_VR_GetVRInitErrorAsEnglishDescription = nullptr;

template <class T>
bool resolve(T& slot, const char* name) {
    slot = util::proc<T>(g_dll, name);
    if (!slot) {
        const DWORD error = ::GetLastError();
        diag::logf("openvr: GetProcAddress({}) failed err={}", name, error);
        // Not a runtime error: name the missing export and give the Windows
        // code, so the message does not read "unknown error, 0" (13 §5.3).
        set_init_failure(std::string{"openvr_api.dll has no export "} + name, static_cast<int>(error));
        return false;
    }
    return true;
}

const char* err_text(vr::EVRInitError e) {
    if (g_VR_GetVRInitErrorAsEnglishDescription) return g_VR_GetVRInitErrorAsEnglishDescription(e);
    return "<error description unavailable>";
}

void unload() {
    if (g_dll) {
        ::FreeLibrary(g_dll);
        g_dll = nullptr;
    }
    if (g_vrclient) {
        ::FreeLibrary(g_vrclient);
        g_vrclient = nullptr;
    }
    g_VR_InitInternal2 = nullptr;
    g_VR_ShutdownInternal = nullptr;
    g_VR_GetGenericInterface = nullptr;
    g_VR_GetVRInitErrorAsEnglishDescription = nullptr;
}

} // namespace

bool init(const std::filesystem::path& runtime_root, win32::ArchiveFiles& payloads) noexcept {
    const auto bin_dir = runtime_root / "bin" / "win32";
    // Prefer the openvr_api.dll 1.0.10 embedded in this DLL, written next to
    // the game now, like the original (which delay-loads it from the app
    // dir). Fall back to the loader that ships with SteamVR.
    payloads.deploy_openvr_api();
    std::error_code ec;
    const auto bundled  = diag::module_directory() / "openvr_api.dll";
    const auto dll_path = std::filesystem::exists(bundled, ec) ? bundled : bin_dir / "openvr_api.dll";

    vr::EVRInitError err = vr::VRInitError_None;
    {
        // The SteamVR bin/win32 directory goes on the DLL search path while
        // openvr_api.dll loads and VR_InitInternal2 runs: both LoadLibrary
        // "vrclient.dll", and it and its dependencies live there. Without it
        // the search fails with error 102 (VRInitError_Init_VRClientDLLNotFound).
        // After VR_InitInternal2, vrclient is mapped and needs no hint.
        const DllDirectoryScope search(bin_dir);

        g_dll = ::LoadLibraryW(dll_path.c_str());
        if (!g_dll) {
            const DWORD error = ::GetLastError();
            diag::logf("openvr: LoadLibrary failed at {} err=0x{:08X}", diag::utf8(dll_path), error);
            set_init_failure("cannot load " + diag::utf8(dll_path), static_cast<int>(error));
            return false;
        }
        diag::logf("openvr: loaded {}", diag::utf8(dll_path));

        if (!resolve(g_VR_InitInternal2,                      "VR_InitInternal2") ||
            !resolve(g_VR_ShutdownInternal,                   "VR_ShutdownInternal") ||
            !resolve(g_VR_GetGenericInterface,                "VR_GetGenericInterface") ||
            !resolve(g_VR_GetVRInitErrorAsEnglishDescription, "VR_GetVRInitErrorAsEnglishDescription"))
        {
            unload();
            return false;
        }
        diag::log_line("openvr: entry points resolved");

        // Pre-load vrclient.dll before calling VR_InitInternal2 so that if
        // loading fails we get an actionable Windows error code in the log.
        // openvr_api.dll internally calls LoadLibraryA("...SteamVR/bin/vrclient.dll");
        // if vrclient.dll or one of its dependencies is missing we need to know which.
        const auto vrclient_path = runtime_root / "bin" / "vrclient.dll";
        g_vrclient = ::LoadLibraryW(vrclient_path.c_str());
        if (!g_vrclient) {
            diag::logf("openvr: vrclient.dll pre-load FAILED err=0x{:08X} path={}",
                       ::GetLastError(), diag::utf8(vrclient_path));
            // Proceed anyway: VR_InitInternal2 may use a different path.
        } else {
            diag::logf("openvr: vrclient.dll pre-loaded at {:08X}", reinterpret_cast<std::uintptr_t>(g_vrclient));
        }

        const std::uint32_t token = g_VR_InitInternal2(&err, vr::VRApplication_Scene, nullptr);
        if (err == vr::VRInitError_None) diag::logf("openvr: VR_InitInternal2 ok, token={}", token);
    }
    if (err != vr::VRInitError_None) {
        diag::logf("openvr: VR_InitInternal2 failed: {} ({})", static_cast<int>(err), err_text(err));
        set_init_failure(err_text(err), static_cast<int>(err));
        unload();
        return false;
    }

    // The backend queries IVRSystem itself; this only checks that it is
    // served and logs what the headset asks for.
    auto* system = static_cast<vr::IVRSystem*>(g_VR_GetGenericInterface(vr::IVRSystem_Version, &err));
    if (err != vr::VRInitError_None || !system) {
        diag::logf("openvr: GetGenericInterface(IVRSystem) failed: {} ({})",
                   static_cast<int>(err), err_text(err));
        set_init_failure(err_text(err), static_cast<int>(err));
        shutdown();
        return false;
    }

    std::uint32_t w = 0, h = 0;
    system->GetRecommendedRenderTargetSize(&w, &h);
    // IPD: distance between the two eye-to-head origins (x components only:
    // the eyes are offset along x in the head's coordinate frame).
    const vr::HmdMatrix34_t leye = system->GetEyeToHeadTransform(vr::Eye_Left);
    const vr::HmdMatrix34_t reye = system->GetEyeToHeadTransform(vr::Eye_Right);
    diag::logf("openvr: IVRSystem ok, per-eye RT={}x{} IPD={:.4f}m", w, h, reye.m[0][3] - leye.m[0][3]);
    return true;
}

void* generic_interface(const char* version, int* error) noexcept {
    if (error) *error = vr::VRInitError_Init_NotInitialized;
    if (!g_VR_GetGenericInterface) return nullptr;
    vr::EVRInitError err = vr::VRInitError_None;
    void* p = g_VR_GetGenericInterface(version, &err);
    if (error) *error = static_cast<int>(err);
    return err == vr::VRInitError_None ? p : nullptr;
}

void shutdown() noexcept {
    if (g_VR_ShutdownInternal) {
        g_VR_ShutdownInternal();
        diag::log_line("openvr: shut down");
    }
    unload();
}

} // namespace omvr::platform::openvr
