#include "platform/openxr/result_names.h"

#include "core/diag/logger.h"        // R: vr (A1): log through an injected Log&
#include "core/vr/runtime_failure.h" // R: vr (A1): the failure travels in the result
#include "platform/openxr/session.h"

#include <openxr/openxr_reflection.h>

#include <string>

namespace omvr::platform::openxr {

namespace diag = core::diag;
using core::vr::set_init_failure;

const char* result_name(XrResult r) noexcept {
    switch (r) {
#define OMVR_XR_RESULT_CASE(name, value) case name: return #name;
        XR_LIST_ENUM_XrResult(OMVR_XR_RESULT_CASE)
#undef OMVR_XR_RESULT_CASE
        default: return "unknown XrResult";
    }
}

const char* state_name(XrSessionState s) noexcept {
    switch (s) {
#define OMVR_XR_STATE_CASE(name, value) case name: return #name;
        XR_LIST_ENUM_XrSessionState(OMVR_XR_STATE_CASE)
#undef OMVR_XR_STATE_CASE
        default: return "unknown XrSessionState";
    }
}

// The result's text, never empty (09 §7).
void Session::result_text(XrResult r, char (&text)[XR_MAX_RESULT_STRING_SIZE]) noexcept {
    text[0] = '\0';
    if (g.instance != XR_NULL_HANDLE) xrResultToString(g.instance, r, text);
    if (text[0] == '\0') strncpy_s(text, result_name(r), _TRUNCATE);
}

// Start-up and session calls: a failure is logged and becomes the reason the
// player's start-up message gives.
bool Session::check(XrResult r, const char* what) {
    if (XR_SUCCEEDED(r)) return true;
    char text[XR_MAX_RESULT_STRING_SIZE];
    result_text(r, text);
    diag::logf("openxr: {} failed: {} ({})", what, static_cast<int>(r), text);
    set_init_failure(std::string{what} + ": " + text, static_cast<int>(r));
    return false;
}

// Calls made every frame (frame functions, swapchain images): logged when
// the result changes, never on every frame, and without allocating (11 §3,
// 12 §6.1). A failure during frames is not a start-up failure, so the
// player's start-up message is left alone.
bool Session::check_frame(XrResult r, const char* what, XrResult& last) noexcept {
    if (r != last) {
        if (XR_FAILED(r)) {
            char text[XR_MAX_RESULT_STRING_SIZE];
            result_text(r, text);
            diag::logf("openxr: {} failed: {} ({})", what, static_cast<int>(r), text);
        } else if (XR_FAILED(last)) {
            diag::logf("openxr: {} succeeds again", what);
        }
        last = r;
    }
    return XR_SUCCEEDED(r);
}

} // namespace omvr::platform::openxr
