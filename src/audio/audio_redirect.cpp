#include "audio_redirect.h"

#include "diag/log.h"
#include "hook/ai_addresses.h"
#include "hook/memory_patch.h"

#include <windows.h>
#include <mmdeviceapi.h>

#include <cstdint>
#include <format>

namespace mothervr::audio {

namespace {

using ActivateFn = HRESULT(__stdcall*)(IMMDevice*, REFIID, DWORD, PROPVARIANT*, void**);

// IMMDevice::Activate is slot 3 of the vtable every MMDevAPI device shares.
constexpr std::size_t kActivateSlot = 3;

ActivateFn   g_activate = nullptr; // the original; set once, with g_endpoint
std::wstring g_endpoint;

HRESULT __stdcall hook_activate(IMMDevice* self, REFIID iid, DWORD clsctx, PROPVARIANT* params, void** out) {
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

HRESULT install() noexcept {
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
    const bool patched = hook::write_dword(
        &vtable[kActivateSlot], static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&hook_activate)));
    const HRESULT result = patched ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
    device->Release();
    return result;
}

} // namespace

void redirect_to(std::wstring endpoint_id) noexcept {
    if (endpoint_id.empty() || g_activate) return;
    g_endpoint = std::move(endpoint_id);
    if (const HRESULT hr = install(); FAILED(hr)) {
        g_activate = nullptr;
        diag::message_box(std::format("Failed to hook Audio!\n[0x{:X}]", static_cast<unsigned>(hr)),
                          MB_OK | MB_ICONERROR);
        return;
    }
    diag::log_line("audio: game output redirected to the headset");

    // The game reopens its audio output, now through the hook.
    if (auto* reopen = hook::ai_address(hook::AiAddr::AudioReopenOutput))
        reinterpret_cast<void(__cdecl*)()>(reopen)();
}

} // namespace mothervr::audio
