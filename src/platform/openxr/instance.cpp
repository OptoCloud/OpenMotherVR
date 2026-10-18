// OpenMotherVR-Provenance: clean
// The OpenXR instance and system: what the stereo device needs before the
// game has a D3D11 device (09 §2.2).

#include "platform/openxr/constants.h"
#include "platform/openxr/session_impl.h"

#include <cstring>
#include <string_view>
#include <vector>

namespace omvr::platform::openxr {

static_assert(kSystemNameSize == std::size_t{XR_MAX_SYSTEM_NAME_SIZE});
static_assert(kRuntimeNameSize == std::size_t{XR_MAX_RUNTIME_NAME_SIZE});

namespace {

using core::diag::ports::logf;

constexpr std::string_view kLogRuntime = "openxr: runtime {} {}.{}.{}";
constexpr std::string_view kLogSystem  = "openxr: {} per eye {}x{}, FOV {}";

// The instance extensions the runtime offers.
std::vector<XrExtensionProperties> offered_extensions() {
    std::uint32_t n = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &n, nullptr);
    std::vector<XrExtensionProperties> props(n, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, n, &n, props.data());
    return props;
}

bool offered(const std::vector<XrExtensionProperties>& props, const char* name) noexcept {
    for (const auto& p : props)
        if (std::strcmp(p.extensionName, name) == 0) return true;
    return false;
}

} // namespace

bool Session::Impl::create_instance(const std::vector<const char*>& extensions) {
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    strncpy_s(ci.applicationInfo.applicationName, kApplicationName, _TRUNCATE);
    strncpy_s(ci.applicationInfo.engineName, kEngineName, _TRUNCATE);
    ci.applicationInfo.applicationVersion = kApplicationVersion;
    ci.applicationInfo.engineVersion      = kEngineVersion;
    ci.applicationInfo.apiVersion         = XR_API_VERSION_1_0;
    ci.enabledExtensionCount              = static_cast<std::uint32_t>(extensions.size());
    ci.enabledExtensionNames              = extensions.data();
    return check(xrCreateInstance(&ci, state.instance.put()), "xrCreateInstance");
}

// The runtime's name and version for the log, and the headset's audio
// output where the runtime names it (10).
void Session::Impl::read_instance_properties(bool audio_ext) {
    XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES};
    if (XR_SUCCEEDED(xrGetInstanceProperties(state.instance.get(), &ip))) {
        logf(log, kLogRuntime, ip.runtimeName, XR_VERSION_MAJOR(ip.runtimeVersion),
             XR_VERSION_MINOR(ip.runtimeVersion), XR_VERSION_PATCH(ip.runtimeVersion));
        strncpy_s(state.info.runtime_name, ip.runtimeName, _TRUNCATE);
    }
    PFN_xrGetAudioOutputDeviceGuidOculus get_audio_output = nullptr;
    if (audio_ext &&
        XR_SUCCEEDED(xrGetInstanceProcAddr(state.instance.get(), "xrGetAudioOutputDeviceGuidOculus",
                                           reinterpret_cast<PFN_xrVoidFunction*>(&get_audio_output)))) {
        wchar_t id[XR_MAX_AUDIO_DEVICE_STR_SIZE_OCULUS] = {};
        if (XR_SUCCEEDED(get_audio_output(state.instance.get(), id))) state.info.audio_output = id;
    }
}

// The headset system, its name, and each eye's recommended size and, with
// XR_EPIC_view_configuration_fov, its FOV (09 §4.1).
bool Session::Impl::read_system(bool fov_ext) {
    XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
    si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!check(xrGetSystem(state.instance.get(), &si, &state.system), "xrGetSystem (is the headset on?)")) {
        return false;
    }
    XrSystemProperties sp{XR_TYPE_SYSTEM_PROPERTIES};
    if (XR_SUCCEEDED(xrGetSystemProperties(state.instance.get(), state.system, &sp))) {
        strncpy_s(state.info.system_name, sp.systemName, _TRUNCATE);
    }

    std::array<XrViewConfigurationViewFovEPIC, core::vr::kEyeCount> fov{
        {{XR_TYPE_VIEW_CONFIGURATION_VIEW_FOV_EPIC}, {XR_TYPE_VIEW_CONFIGURATION_VIEW_FOV_EPIC}}};
    state.config_views.fill({XR_TYPE_VIEW_CONFIGURATION_VIEW});
    if (fov_ext) {
        state.config_views[0].next = &fov[0];
        state.config_views[1].next = &fov[1];
    }
    std::uint32_t n = 0;
    if (!check(xrEnumerateViewConfigurationViews(
                   state.instance.get(), state.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                   static_cast<std::uint32_t>(state.config_views.size()), &n, state.config_views.data()),
               "xrEnumerateViewConfigurationViews")) {
        return false;
    }
    state.config_views[0].next = state.config_views[1].next = nullptr;
    state.info.recommended_w                                = state.config_views[0].recommendedImageRectWidth;
    state.info.recommended_h = state.config_views[0].recommendedImageRectHeight;
    state.info.has_fov       = fov_ext;
    if (fov_ext) state.info.fov = {from_xr(fov[0].recommendedFov), from_xr(fov[1].recommendedFov)};
    logf(log, kLogSystem, state.info.system_name, state.info.recommended_w, state.info.recommended_h,
         fov_ext ? "reported" : "not reported (from the first frame)");
    return true;
}

bool Session::Impl::init_system() noexcept {
    if (state.instance) return true;

    // Instance with the D3D11 binding, and the per-eye FOV query where the
    // runtime offers it (the FOV is needed before a session can exist).
    set_runtime_environment(manifest);
    point_loader_at(manifest);
    const auto props     = offered_extensions();
    const bool fov_ext   = offered(props, XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME);
    const bool audio_ext = offered(props, XR_OCULUS_AUDIO_DEVICE_GUID_EXTENSION_NAME);
    // 22 §6.1: LOCAL_FLOOR is the floor reference when there is no STAGE.
    const bool floor_ext = offered(props, XR_EXT_LOCAL_FLOOR_EXTENSION_NAME);
    // 04 §5.8.4: the Steam Frame's controller profile needs its extension.
    const bool frame_ext = offered(props, kFrameControllerExtensionName);
    std::vector<const char*> extensions{XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
    if (fov_ext) extensions.push_back(XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME);
    if (audio_ext) extensions.push_back(XR_OCULUS_AUDIO_DEVICE_GUID_EXTENSION_NAME);
    if (floor_ext) extensions.push_back(XR_EXT_LOCAL_FLOOR_EXTENSION_NAME);
    if (frame_ext) extensions.push_back(kFrameControllerExtensionName);
    if (!create_instance(extensions)) {
        teardown();
        return false;
    }
    input.set_frame_profile(frame_ext);
    read_instance_properties(audio_ext);
    if (!read_system(fov_ext)) {
        teardown();
        return false;
    }
    return true;
}

} // namespace omvr::platform::openxr
