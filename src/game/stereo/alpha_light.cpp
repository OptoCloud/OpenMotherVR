// OpenMotherVR-Provenance: clean
#include "game/stereo/alpha_light.h"

#include "game/code_address.h"
#include "game/field.h"

#include <cstddef>

namespace omvr::game::stereo {

namespace {

using EyeNdcFn = float*(__fastcall*)(void* manager, void* edx, float* out, std::int32_t eye);

// 08 §8.1: the atlas camera's axes at +0xF0 to +0x110 (the forward one
// last) and its position at +0x120.
constexpr std::ptrdiff_t kAtlasForward  = 0x110;
constexpr std::ptrdiff_t kAtlasPosition = 0x120;
// 24 §14.2: the view the camera-constant setter stores, the drawn POV's.
constexpr std::ptrdiff_t kDrawnView = 0x280;

} // namespace

float* call_eye_ndc(const AlphaLightTargets& targets, void* manager, float* out, std::int32_t eye) noexcept {
    const EyeNdcFn eye_ndc = CodeAddress{targets.eye_ndc};
    return eye_ndc(manager, nullptr, out, eye);
}

std::optional<AtlasCamera> read_atlas_camera(const AlphaLightTargets& targets) noexcept {
    if (targets.light_object == nullptr) return std::nullopt;
    return AtlasCamera{.forward  = read_field<util::Vec3>(targets.light_object, kAtlasForward),
                       .position = read_field<util::Vec3>(targets.light_object, kAtlasPosition)};
}

std::optional<util::Matrix44> read_drawn_view(const AlphaLightTargets& targets) noexcept {
    if (targets.camera_constants == nullptr) return std::nullopt;
    return read_field<util::Matrix44>(targets.camera_constants, kDrawnView);
}

} // namespace omvr::game::stereo
