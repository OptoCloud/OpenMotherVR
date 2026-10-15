#include "platform/audio/endpoint_redirect.h"

#include "platform/audio/constants.h"
#include "platform/win32/patching/memory.h"

#include <windows.h>
#include <mmdeviceapi.h>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace omvr::platform::audio {

static_assert(std::is_same_v<EndpointRedirect::ActivateFn,
                             HRESULT(__stdcall*)(IMMDevice*, REFIID, DWORD, PROPVARIANT*, void**)>);

long EndpointRedirect::activate(IMMDevice* self, REFIID iid, DWORD clsctx, PROPVARIANT* params, void** out) noexcept {
    IMMDeviceEnumerator* enumerator = nullptr;
    if (SUCCEEDED(::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                     IID_PPV_ARGS(&enumerator)))) {
        IMMDevice* headset = nullptr;
        const HRESULT found = enumerator->GetDevice(g_endpoint.c_str(), &headset);
        enumerator->Release();
        if (SUCCEEDED(found)) {
            const HRESULT hr = g_activate(headset, iid, clsctx, params, out);
            headset->Release();
            return hr;
        }
    }
    return g_activate(self, iid, clsctx, params, out); // headset gone: the game's choice
}

long EndpointRedirect::install(std::wstring endpoint_id, ActivateFn replacement) noexcept {
    g_endpoint = std::move(endpoint_id);
    // The game has COM up on this thread already; a mismatched apartment
    // (RPC_E_CHANGED_MODE) still leaves it usable. Not uninitialised: the
    // patched vtable must outlive this call.
    (void) ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                    IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) return hr;
    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    enumerator->Release();
    if (FAILED(hr)) return hr;

    auto** vtable = *reinterpret_cast<void***>(device);
    g_activate = reinterpret_cast<ActivateFn>(vtable[kActivateSlot]);
    const auto value = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(replacement));
    const bool patched = win32::write_memory(&vtable[kActivateSlot], &value, sizeof(value));
    const HRESULT result = patched ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
    device->Release();
    if (FAILED(result)) g_activate = nullptr;
    return result;
}

} // namespace omvr::platform::audio
