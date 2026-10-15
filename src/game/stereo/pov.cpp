// Analysis: dumps/07_reflections_head_pose.md.

#include "game/stereo/pov.h"

#include "game/addresses.h"
#include "game/stereo/follow_camera.h"

#include <cmath>
#include <cstring>

namespace omvr::game::stereo {

const float* view_matrix() noexcept {
    auto* pov = *reinterpret_cast<std::uint8_t**>(ai_address(AiAddr::PovManagerPtr));
    return pov ? field_ptr<float>(pov, 8) : nullptr;
}

void setup_eye(const ReflectionTargets& g_reflection, StereoManager* manager, std::uint8_t* pov, int eye) noexcept {
    using ApplyFn   = void (__fastcall*)(void*, void*, int, float*, float*);
    using FrustumFn = void (__fastcall*)(void*, void*);
    field<std::int32_t>(pov, kPovEye) = eye;
    reinterpret_cast<ApplyFn>(g_reflection.apply_eye_transform)(
        manager, nullptr, eye, &field<float>(pov, kPovView), &field<float>(pov, kPovProj));
    reinterpret_cast<FrustumFn>(g_reflection.update_frustum)(pov, nullptr);
    const float* proj = &field<float>(pov, kPovProj);
    field<float>(pov, kPovAspect) = proj[5] / proj[0];
    field<float>(pov, kPovFov) = 2.0f * std::atan(1.0f / proj[5]);
    std::memcpy(pov + kPovProjCopy, proj, 0x40);
    if (auto* owner = static_cast<std::uint8_t*>(*g_reflection.renderer_eye_owner)) {
        field<std::int32_t>(owner, 0x2C) = eye;
    }
}

} // namespace omvr::game::stereo
