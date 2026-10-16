// OpenMotherVR-Provenance: clean
#include "game/stereo/alpha_light.h"

#include "game/code_address.h"

namespace omvr::game::stereo {

namespace {

using EyeNdcFn = float*(__fastcall*)(void* manager, void* edx, float* out, std::int32_t eye);

} // namespace

float* call_eye_ndc(const AlphaLightTargets& targets, void* manager, float* out, std::int32_t eye) noexcept {
    const EyeNdcFn eye_ndc = CodeAddress{targets.eye_ndc};
    return eye_ndc(manager, nullptr, out, eye);
}

} // namespace omvr::game::stereo
