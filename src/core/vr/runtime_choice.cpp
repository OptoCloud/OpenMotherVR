#include "core/vr/runtime_choice.h"

namespace omvr::core::vr {

const char* to_string(detect_result::Chosen c) noexcept {
    switch (c) {
        case detect_result::Chosen::OpenVR: return "OpenVR";
        case detect_result::Chosen::Oculus: return "Oculus";
        case detect_result::Chosen::OpenXR: return "OpenXR";
        case detect_result::Chosen::None:   return "None";
    }
    return "None";
}

} // namespace omvr::core::vr
