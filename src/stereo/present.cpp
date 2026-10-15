// Per-frame Present hook: the OpenXR virtual-screen path.

#include "present.h"

#include "diag/log.h"
#include "integration/script_bridge.h"
#include "runtime/openxr_backend.h"
#include "runtime/vr_detect.h"
#include "stereo_device.h"

#include <d3d11.h>
#include <windows.h>

#include <mutex>

namespace mothervr::stereo {

bool on_present(IDXGISwapChain* swapchain) noexcept {
    // Only when OpenXR could not get a stereo device: the game's frame on a
    // virtual screen. With a stereo device the backend drives the frames.
    if (runtime::last_result().chosen != runtime::detect_result::Chosen::OpenXR) return false;
    if (load_device(manager())) return false;
    // The stereo device gave up on the runtime and the player was told VR
    // is off; do not start it again here for the virtual screen.
    if (runtime_failed()) return false;

    // The session is bound to the game's own device so the game's images can
    // be copied into runtime swapchains without sharing.
    static std::once_flag s_init;
    std::call_once(s_init, [swapchain]() {
        diag::log_line("[stereo] starting OpenXR session");
        ID3D11Device* device = nullptr;
        if (swapchain && SUCCEEDED(swapchain->GetDevice(__uuidof(ID3D11Device),
                                                        reinterpret_cast<void**>(&device)))) {
            const auto si = runtime::openxr::init(device);
            device->Release(); // the backend holds its own reference
            integration::g_vr_session_active = si.ok;
            if (!si.ok) diag::log_line("[stereo] OpenXR init failed, running without VR");
        }
    });

    // Ctrl+Alt recenters, as in the original (readme: "Recalibration").
    static bool s_recenter_held = false;
    const bool held = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) && (::GetAsyncKeyState(VK_MENU) & 0x8000);
    if (held && !s_recenter_held) runtime::openxr::recenter();
    s_recenter_held = held;

    // xrWaitFrame paces the game here; the real Present must not wait for
    // the monitor's vsync as well.
    return runtime::openxr::frame(swapchain);
}

} // namespace mothervr::stereo
