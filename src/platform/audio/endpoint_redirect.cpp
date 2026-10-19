#include "platform/audio/endpoint_redirect.h"

#include "platform/audio/constants.h"
#include "platform/win32/com_ptr.h"
#include "platform/win32/patching/memory.h"

#include <windows.h>
#include <mmdeviceapi.h>

#include <array>
#include <bit>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace omvr::platform::audio {

namespace {

using win32::ComPtr;

static_assert(std::is_same_v<EndpointRedirect::ActivateFn,
                             HRESULT(__stdcall*)(IMMDevice*, REFIID, DWORD, PROPVARIANT*, void**)>);

HRESULT create_enumerator(ComPtr<IMMDeviceEnumerator>& out) noexcept {
    IMMDeviceEnumerator* raw = nullptr;
    const HRESULT hr =
        ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&raw));
    if (SUCCEEDED(hr)) out = ComPtr<IMMDeviceEnumerator>::adopt(raw);
    return hr;
}

// The headset's device, or empty when it is gone.
ComPtr<IMMDevice> device_by_id(const std::wstring& endpoint) noexcept {
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(create_enumerator(enumerator))) return {};
    IMMDevice* raw = nullptr;
    if (FAILED(enumerator->GetDevice(endpoint.c_str(), &raw))) return {};
    return ComPtr<IMMDevice>::adopt(raw);
}

// The default render device the game would open, or the failure.
HRESULT default_device(ComPtr<IMMDevice>& out) noexcept {
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = create_enumerator(enumerator);
    if (FAILED(hr)) return hr;
    IMMDevice* raw = nullptr;
    hr             = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &raw);
    if (SUCCEEDED(hr)) out = ComPtr<IMMDevice>::adopt(raw);
    return hr;
}

// The vtable every MMDevAPI device shares (its first word).
void** vtable_of(IMMDevice* device) noexcept { return *reinterpret_cast<void***>(device); }

} // namespace

long EndpointRedirect::activate(IMMDevice* self, REFIID iid, DWORD clsctx, PROPVARIANT* params,
                                void** out) noexcept {
    const ComPtr<IMMDevice> headset = device_by_id(endpoint_);
    if (headset) return original_activate_(headset.get(), iid, clsctx, params, out);
    return original_activate_(self, iid, clsctx, params, out); // headset gone: the game's choice
}

long EndpointRedirect::install(std::wstring endpoint_id, ActivateFn replacement,
                               core::diag::ports::Log& log) noexcept {
    endpoint_ = std::move(endpoint_id);
    // The game has COM up on this thread already; a mismatched apartment
    // (RPC_E_CHANGED_MODE) still leaves it usable. Not uninitialised: the
    // patched vtable must outlive this call.
    static_cast<void>(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

    ComPtr<IMMDevice> device;
    if (const HRESULT hr = default_device(device); FAILED(hr)) return hr;

    void** vtable      = vtable_of(device.get());
    original_activate_ = reinterpret_cast<ActivateFn>(vtable[kActivateSlot]);
    const auto value   = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(replacement));
    const auto bytes   = std::bit_cast<std::array<std::uint8_t, sizeof(value)>>(value);
    // The code captured at the failing call: GetLastError here would be
    // stale after write_memory's logging and protection restore, and a 0
    // would turn the failure into S_OK.
    if (const auto written = win32::write_memory(&vtable[kActivateSlot], bytes, log); !written) {
        original_activate_ = nullptr;
        const auto code    = static_cast<DWORD>(written.error().code);
        return code != 0 ? HRESULT_FROM_WIN32(code) : E_FAIL;
    }
    return S_OK;
}

} // namespace omvr::platform::audio
