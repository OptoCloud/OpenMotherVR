#pragma once

#include "platform/openxr/xr_headers.h"

namespace omvr::platform::openxr {

// xrResultToString needs an instance, and there is none when xrCreateInstance
// itself fails; the names come from the registry header instead.
const char* result_name(XrResult r) noexcept;

// The name of a session state, for the log.
const char* state_name(XrSessionState s) noexcept;

} // namespace omvr::platform::openxr
