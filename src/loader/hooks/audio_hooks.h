#pragma once

#include "platform/audio/endpoint_redirect.h"

namespace omvr::loader::hooks {

// The IMMDevice::Activate replacement.
platform::audio::EndpointRedirect::ActivateFn activate_replacement() noexcept;

} // namespace omvr::loader::hooks
