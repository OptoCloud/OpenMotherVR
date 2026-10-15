// The OpenXR instance and system: what the stereo device needs before the
// game has a D3D11 device.

#include "core/diag/logger.h" // R: vr (A1): log through an injected Log&
#include "platform/openxr/constants.h"
#include "platform/openxr/runtime_probe.h"
#include "platform/openxr/session.h"

#include <cstring>
#include <vector>

namespace omvr::platform::openxr {

namespace diag = core::diag;

bool Session::init_system() noexcept {
    if (g.instance != XR_NULL_HANDLE) return true;

    // Instance with the D3D11 binding, and the per-eye FOV query where the
    // runtime offers it (the FOV is needed before a session can exist).
    set_runtime_environment(g_manifest);
    point_loader_at(g_manifest);
    std::uint32_t n_ext = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &n_ext, nullptr);
    std::vector<XrExtensionProperties> props(n_ext, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, n_ext, &n_ext, props.data());
    const auto offered = [&](const char* name) {
        for (const auto& p : props)
            if (std::strcmp(p.extensionName, name) == 0) return true;
        return false;
    };
    const bool fov_ext   = offered(XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME);
    const bool audio_ext = offered(XR_OCULUS_AUDIO_DEVICE_GUID_EXTENSION_NAME);
    std::vector<const char*> extensions{XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
    if (fov_ext) extensions.push_back(XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME);
    if (audio_ext) extensions.push_back(XR_OCULUS_AUDIO_DEVICE_GUID_EXTENSION_NAME);
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    strncpy_s(ci.applicationInfo.applicationName, kApplicationName, _TRUNCATE);
    strncpy_s(ci.applicationInfo.engineName, kEngineName, _TRUNCATE);
    ci.applicationInfo.applicationVersion = kApplicationVersion;
    ci.applicationInfo.engineVersion      = kEngineVersion;
    ci.applicationInfo.apiVersion         = XR_API_VERSION_1_0;
    ci.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    ci.enabledExtensionNames = extensions.data();
    if (!check(xrCreateInstance(&ci, &g.instance), "xrCreateInstance")) {
        teardown();
        return false;
    }
    XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES};
    if (XR_SUCCEEDED(xrGetInstanceProperties(g.instance, &ip))) {
        diag::logf("openxr: runtime {} {}.{}.{}", ip.runtimeName,
                   XR_VERSION_MAJOR(ip.runtimeVersion), XR_VERSION_MINOR(ip.runtimeVersion),
                   XR_VERSION_PATCH(ip.runtimeVersion));
        strncpy_s(g.info.runtime_name, ip.runtimeName, _TRUNCATE);
    }
    PFN_xrGetAudioOutputDeviceGuidOculus get_audio_output = nullptr;
    if (audio_ext &&
        XR_SUCCEEDED(xrGetInstanceProcAddr(g.instance, "xrGetAudioOutputDeviceGuidOculus",
                                           reinterpret_cast<PFN_xrVoidFunction*>(&get_audio_output)))) {
        wchar_t id[XR_MAX_AUDIO_DEVICE_STR_SIZE_OCULUS] = {};
        if (XR_SUCCEEDED(get_audio_output(g.instance, id))) g.info.audio_output = id;
    }

    XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
    si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!check(xrGetSystem(g.instance, &si, &g.system), "xrGetSystem (is the headset on?)")) {
        teardown();
        return false;
    }
    XrSystemProperties sp{XR_TYPE_SYSTEM_PROPERTIES};
    if (XR_SUCCEEDED(xrGetSystemProperties(g.instance, g.system, &sp))) {
        strncpy_s(g.info.system_name, sp.systemName, _TRUNCATE);
    }

    std::array<XrViewConfigurationViewFovEPIC, 2> fov{{{XR_TYPE_VIEW_CONFIGURATION_VIEW_FOV_EPIC},
                                                       {XR_TYPE_VIEW_CONFIGURATION_VIEW_FOV_EPIC}}};
    g.config_views.fill({XR_TYPE_VIEW_CONFIGURATION_VIEW});
    if (fov_ext) {
        g.config_views[0].next = &fov[0];
        g.config_views[1].next = &fov[1];
    }
    std::uint32_t n = 0;
    if (!check(xrEnumerateViewConfigurationViews(g.instance, g.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                 2, &n, g.config_views.data()),
               "xrEnumerateViewConfigurationViews")) {
        teardown();
        return false;
    }
    g.config_views[0].next = g.config_views[1].next = nullptr;
    g.info.recommended_w = g.config_views[0].recommendedImageRectWidth;
    g.info.recommended_h = g.config_views[0].recommendedImageRectHeight;
    g.info.has_fov = fov_ext;
    if (fov_ext) g.info.fov = {fov[0].recommendedFov, fov[1].recommendedFov};
    diag::logf("openxr: {} per eye {}x{}, FOV {}", g.info.system_name, g.info.recommended_w,
               g.info.recommended_h, fov_ext ? "reported" : "not reported (from the first frame)");
    return true;
}

const SystemInfo& Session::system_info() const noexcept { return g.info; }

} // namespace omvr::platform::openxr
