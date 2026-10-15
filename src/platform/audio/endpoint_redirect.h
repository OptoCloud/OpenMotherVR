#pragma once

#include "core/diag/ports/log.h"

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
    bool installed() const noexcept { return original_activate_ != nullptr; }

    // Hooks the shared IMMDevice vtable's Activate with `replacement`, to
    // activate `endpoint_id` (an MMDevice endpoint ID) instead. Returns the
    // HRESULT; on failure nothing is hooked. The thread that starts the
    // runtime, which has COM up. A failed slot write is logged to `log`.
    long install(std::wstring endpoint_id, ActivateFn replacement, core::diag::ports::Log& log) noexcept;

    // The replacement's work: the headset's device if it is still there,
    // else the game's choice. COM calls it on the game's audio threads.
    long activate(IMMDevice* self, const _GUID& iid, unsigned long clsctx, tagPROPVARIANT* params,
                  void** out) noexcept;

private:
    // Set once, with endpoint_, before the slot is written; only read after.
    ActivateFn original_activate_ = nullptr;
    std::wstring endpoint_;
};

} // namespace omvr::platform::audio
