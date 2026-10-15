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

// The reopen function's entry as verified on the 2015 build:
// `mov byte [flag], 1` (C6 05 <abs32> 01), then `call` (E8) the engine's
// "initialised" getter. Anything else is not the function this table entry
// names (another build, or the unverified 2020 address), and calling it
// would run arbitrary game code.
bool reopen_entry_matches(const std::uint8_t* p) noexcept {
    return p[0] == 0xC6 && p[1] == 0x05 && p[6] == 0x01 && p[7] == 0xE8;
}

} // namespace

void redirect_to(std::wstring endpoint_id) noexcept {
    if (endpoint_id.empty() || g_activate) return;
    g_endpoint = std::move(endpoint_id);
    if (const HRESULT hr = install(); FAILED(hr)) {
        g_activate = nullptr;
        diag::logf("audio: game output stays on the default device: redirect failed hr=0x{:08X}",
                   static_cast<unsigned>(hr));
        diag::message_box(std::format("Failed to hook Audio!\n[0x{:X}]", static_cast<unsigned>(hr)),
                          MB_OK | MB_ICONERROR, "audio failed");
        return;
    }
    diag::log_line("audio: game output redirected to the headset");

    // The game reopens its audio output, now through the hook. Its address
    // is in the table for both builds; an RVA of 0 (unknown) gives the image
    // base, whose bytes ("MZ") do not match.
    std::uint8_t* reopen = hook::ai_address(hook::AiAddr::AudioReopenOutput);
    if (!reopen || !reopen_entry_matches(reopen)) {
        diag::log_line("audio: output reopen not available (entry bytes differ); "
                       "switch happens at the game's next reopen");
        return;
    }
    reinterpret_cast<void(__cdecl*)()>(reopen)();
}

} // namespace mothervr::audio
