// OpenVR session bring-up (03 §6.4). The IVRSystem* that
// VR_GetGenericInterface returns is the C++ vtable form: openvr.h declares
// it as a class with virtuals, and the SDK's vtable layout is MSVC's, so
// calling its members from this MSVC build is well defined.

#include "platform/openvr/api_loader.h"

#include "core/diag/ports/log.h"
#include "core/vr/runtime_failure.h"
#include "platform/openvr/constants.h"
#include "platform/win32/dll_directory_scope.h"
#include "platform/win32/module.h"
#include "platform/win32/payload_files.h"
#include "platform/win32/proc.h"
#include "platform/win32/text.h"

#include <openvr.h>
#include <windows.h>

#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace omvr::platform::openvr {

namespace {

using core::diag::ports::logf;
using win32::utf8;

constexpr std::string_view kLogNoExport        = "openvr: GetProcAddress({}) failed err={}";
constexpr std::string_view kLogLoadFailed      = "openvr: LoadLibrary failed at {} err=0x{:08X}";
constexpr std::string_view kLogLoaded          = "openvr: loaded {}";
constexpr std::string_view kLogResolved        = "openvr: entry points resolved";
constexpr std::string_view kLogVrClientFailed  = "openvr: vrclient.dll pre-load FAILED err=0x{:08X} path={}";
constexpr std::string_view kLogVrClientLoaded  = "openvr: vrclient.dll pre-loaded at {:08X}";
constexpr std::string_view kLogInitOk          = "openvr: VR_InitInternal2 ok, token={}";
constexpr std::string_view kLogInitFailed      = "openvr: VR_InitInternal2 failed: {} ({})";
constexpr std::string_view kLogSystemFailed    = "openvr: GetGenericInterface(IVRSystem) failed: {} ({})";
constexpr std::string_view kLogSystemOk        = "openvr: IVRSystem ok, per-eye RT={}x{} IPD={:.4f}m";
constexpr std::string_view kLogShutDown        = "openvr: shut down";

// Texts of the player's start-up failure message (13 §5.3).
constexpr std::string_view kFailNoExport       = "openvr_api.dll has no export ";
constexpr std::string_view kFailCannotLoad     = "cannot load ";
constexpr const char*      kNoErrorDescription = "<error description unavailable>";

// The Error context of a missing interface.
constexpr std::string_view kGenericInterfaceCall = "VR_GetGenericInterface";

using InitInternal2Fn       = std::uint32_t(__cdecl*)(vr::EVRInitError*, vr::EVRApplicationType, const char*);
using ShutdownInternalFn    = void(__cdecl*)();
using GetGenericInterfaceFn = void*(__cdecl*)(const char*, vr::EVRInitError*);
using ErrorDescriptionFn    = const char*(__cdecl*)(vr::EVRInitError);

// The loader's exports the mod uses (03 §6.4).
struct Entries {
    InitInternal2Fn       init_internal2        = nullptr;
    ShutdownInternalFn    shutdown_internal     = nullptr;
    GetGenericInterfaceFn get_generic_interface = nullptr;
    ErrorDescriptionFn    error_description     = nullptr;
};

// The deployed copy next to the game when it is there, else SteamVR's own.
std::filesystem::path loader_path(const std::filesystem::path& bin_dir) {
    std::error_code ec;
    const auto bundled = win32::module_directory() / kLoaderFileName;
    return std::filesystem::exists(bundled, ec) ? bundled : bin_dir / kLoaderFileName;
}

} // namespace

struct ApiLoader::State {
    win32::UniqueModule api;        // openvr_api.dll
    win32::UniqueModule vrclient;   // our extra reference from the pre-load
    Entries      entries{};
};

ApiLoader::ApiLoader(core::diag::ports::Log& log, core::vr::RuntimeFailure& failure) noexcept
    : log_(log), failure_(failure), state_(std::make_unique<State>()) {}

ApiLoader::~ApiLoader() { unload(); }

const char* ApiLoader::error_text(int error) const noexcept {
    if (state_->entries.error_description) {
        return state_->entries.error_description(static_cast<vr::EVRInitError>(error));
    }
    return kNoErrorDescription;
}

void ApiLoader::unload() noexcept {
    // The loader first, then the extra vrclient reference.
    state_->api.reset();
    state_->vrclient.reset();
    state_->entries = {};
}

bool ApiLoader::load(const std::filesystem::path& dll_path) noexcept {
    state_->api.reset(::LoadLibraryW(dll_path.c_str()));
    if (!state_->api) {
        const DWORD error = ::GetLastError();
        logf(log_, kLogLoadFailed, utf8(dll_path), error);
        failure_.report(std::string{kFailCannotLoad} + utf8(dll_path), static_cast<std::int32_t>(error));
        return false;
    }
    logf(log_, kLogLoaded, utf8(dll_path));
    return true;
}

bool ApiLoader::resolve_entries() noexcept {
    const auto resolve = [this](auto& slot, const char* name) {
        slot = win32::proc<std::remove_reference_t<decltype(slot)>>(state_->api.get(), name);
        if (slot) return true;
        const DWORD error = ::GetLastError();
        logf(log_, kLogNoExport, name, error);
        // Not a runtime error: name the missing export and give the Windows
        // code, so the message does not read "unknown error, 0" (13 §5.3).
        failure_.report(std::string{kFailNoExport} + name, static_cast<std::int32_t>(error));
        return false;
    };
    Entries& e = state_->entries;
    return resolve(e.init_internal2, "VR_InitInternal2") && resolve(e.shutdown_internal, "VR_ShutdownInternal") &&
           resolve(e.get_generic_interface, "VR_GetGenericInterface") &&
           resolve(e.error_description, "VR_GetVRInitErrorAsEnglishDescription");
}

// The loader loads vrclient.dll itself; loading it first, with the same
// search path, puts the Windows error of a missing file or dependency in the
// log instead of only VRInitError_Init_VRClientDLLNotFound (03 §6.4).
void ApiLoader::preload_vrclient(const std::filesystem::path& runtime_root) noexcept {
    const auto vrclient_path = runtime_root / kBinDirectory / kVrClientFileName;
    state_->vrclient.reset(::LoadLibraryW(vrclient_path.c_str()));
    if (!state_->vrclient) {
        const DWORD error = ::GetLastError();
        logf(log_, kLogVrClientFailed, error, utf8(vrclient_path));
        return;   // VR_InitInternal2 may still find it another way
    }
    logf(log_, kLogVrClientLoaded, std::bit_cast<std::uintptr_t>(state_->vrclient.get()));
}

bool ApiLoader::init(const std::filesystem::path& runtime_root, win32::ArchiveFiles& payloads) noexcept {
    const auto bin_dir = runtime_root / kBinDirectory / kX86Directory;
    // Prefer the openvr_api.dll embedded in this DLL, written next to the
    // game now (13); fall back to the loader that ships with SteamVR.
    payloads.deploy_openvr_api();
    const auto dll_path = loader_path(bin_dir);

    vr::EVRInitError err = vr::VRInitError_None;
    {
        // bin\win32 is on the DLL search path while the loader loads and
        // VR_InitInternal2 runs: both load vrclient.dll, and its x86
        // dependencies live there (03 §6.4). Afterwards vrclient is mapped
        // and needs no hint.
        const win32::DllDirectoryScope search(bin_dir);
        if (!load(dll_path)) return false;
        if (!resolve_entries()) {
            unload();
            return false;
        }
        log_.line(kLogResolved);
        preload_vrclient(runtime_root);
        const std::uint32_t token = state_->entries.init_internal2(&err, vr::VRApplication_Scene, nullptr);
        if (err == vr::VRInitError_None) logf(log_, kLogInitOk, token);
    }
    if (err != vr::VRInitError_None) {
        logf(log_, kLogInitFailed, static_cast<int>(err), error_text(err));
        failure_.report(error_text(err), static_cast<std::int32_t>(err));
        unload();
        return false;
    }
    return check_system();
}

// The stereo device queries IVRSystem itself; this only checks that it is
// served and logs what the headset asks for (03 §6.4, "logged once").
bool ApiLoader::check_system() noexcept {
    vr::EVRInitError err = vr::VRInitError_None;
    auto* system = static_cast<vr::IVRSystem*>(state_->entries.get_generic_interface(vr::IVRSystem_Version, &err));
    if (err != vr::VRInitError_None || !system) {
        logf(log_, kLogSystemFailed, static_cast<int>(err), error_text(err));
        failure_.report(error_text(err), static_cast<std::int32_t>(err));
        shutdown();
        return false;
    }

    std::uint32_t w = 0, h = 0;
    system->GetRecommendedRenderTargetSize(&w, &h);
    // IPD: distance between the two eye-to-head origins (x components only:
    // the eyes are offset along x in the head's coordinate frame).
    const vr::HmdMatrix34_t leye = system->GetEyeToHeadTransform(vr::Eye_Left);
    const vr::HmdMatrix34_t reye = system->GetEyeToHeadTransform(vr::Eye_Right);
    logf(log_, kLogSystemOk, w, h, reye.m[0][3] - leye.m[0][3]);
    return true;
}

util::Result<void*> ApiLoader::generic_interface(const char* version) noexcept {
    if (!state_->entries.get_generic_interface) {
        return std::unexpected(util::Error{vr::VRInitError_Init_NotInitialized, kGenericInterfaceCall});
    }
    vr::EVRInitError err = vr::VRInitError_None;
    void* const p = state_->entries.get_generic_interface(version, &err);
    if (err != vr::VRInitError_None || !p) {
        return std::unexpected(util::Error{static_cast<std::int32_t>(err), kGenericInterfaceCall});
    }
    return p;
}

void ApiLoader::shutdown() noexcept {
    if (state_->entries.shutdown_internal) {
        state_->entries.shutdown_internal();
        log_.line(kLogShutDown);
    }
    unload();
}

} // namespace omvr::platform::openvr
