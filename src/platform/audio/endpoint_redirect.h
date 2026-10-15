#pragma once

#include <string>

struct _GUID;
struct IMMDevice;
struct tagPROPVARIANT;

// Play the game's audio on the headset's own output device: IMMDevice::Activate
// is hooked so whichever device the game opens, the headset's is activated
// instead.
// From: FUN_1001d630 (install), FUN_10001d60 (hook).
namespace omvr::platform::audio {

class EndpointRedirect {
public:
    // IMMDevice::Activate.
    using ActivateFn = long(__stdcall*)(IMMDevice*, const _GUID&, unsigned long, tagPROPVARIANT*, void**);

    // Whether the hook is in (it takes effect once).
    bool installed() const noexcept { return g_activate != nullptr; }

    // Hooks the shared IMMDevice vtable's Activate with `replacement`, to
    // activate `endpoint_id` (an MMDevice endpoint ID) instead. Returns the
    // HRESULT; on failure nothing is hooked.
    long install(std::wstring endpoint_id, ActivateFn replacement) noexcept;

    // The replacement's work: the headset's device if it is still there,
    // else the game's choice.
    long activate(IMMDevice* self, const _GUID& iid, unsigned long clsctx, tagPROPVARIANT* params,
                  void** out) noexcept;

private:
    ActivateFn   g_activate = nullptr; // the original; set once, with g_endpoint
    std::wstring g_endpoint;
};

} // namespace omvr::platform::audio
