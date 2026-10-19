// OpenMotherVR-Provenance: clean
#include "game/hands/arm_objects.h"

#include "game/code_address.h"
#include "game/field.h"
#include "game/hands/body_objects.h"

#include <cstdint>

namespace omvr::game::hands {

namespace {

using core::hands::BoneTransform;
using util::Vec3;

// The game's methods, called as __fastcall with an unused EDX (STYLEGUIDE
// 6): `this` in ECX.
using EquipmentUpdateFn = void(__fastcall*)(void* self, void* edx);

// 21 §2.3: the pose's transforms are read with aligned 16-byte loads.
constexpr std::uintptr_t kTransformAlignment = 16;

Vec3 read_vec3(const void* object, std::ptrdiff_t offset) noexcept {
    return {read_field<float>(object, offset), read_field<float>(object, offset + sizeof(float)),
            read_field<float>(object, offset + 2 * sizeof(float))};
}

bool aligned(const void* p) noexcept { return (address_of(p) % kTransformAlignment) == 0; }

} // namespace

core::hands::ModelToWorld model_to_world(const void* animation) noexcept {
    core::hands::ModelToWorld w;
    w.translation = read_vec3(animation, kAnimationWorldTranslation);
    w.rotation    = {read_field<float>(animation, kAnimationWorldRotation),
                     read_field<float>(animation, kAnimationWorldRotation + sizeof(float)),
                     read_field<float>(animation, kAnimationWorldRotation + 2 * sizeof(float)),
                     read_field<float>(animation, kAnimationWorldRotation + 3 * sizeof(float))};
    w.scale       = read_vec3(animation, kAnimationWorldScale);
    return w;
}

std::optional<core::hands::PoseArrays> pose_arrays(void* pose) noexcept {
    const void* skeleton = pose_skeleton(pose);
    if (!skeleton) return std::nullopt;
    const auto bones = read_field<std::int32_t>(skeleton, kSkeletonBoneCount);
    if (bones <= 0 || read_field<std::int32_t>(pose, kPoseLocalCount) != bones ||
        pose_model_count(pose) != bones || read_field<std::int32_t>(pose, kPoseFlagsCount) != bones)
        return std::nullopt;
    void* local         = pointer_field(pose, kPoseLocalData);
    void* model         = pointer_field(pose, kPoseModelData);
    void* flags         = pointer_field(pose, kPoseFlagsData);
    const void* parents = pointer_field(skeleton, kSkeletonParents);
    if (!local || !model || !flags || !parents || !aligned(local) || !aligned(model)) return std::nullopt;
    const auto n = static_cast<std::size_t>(bones);
    return core::hands::PoseArrays{{static_cast<BoneTransform*>(local), n},
                                   {static_cast<BoneTransform*>(model), n},
                                   {static_cast<std::uint32_t*>(flags), n},
                                   {static_cast<const std::int16_t*>(parents), n}};
}

void* equipment_component(void* character) noexcept {
    if (!character || read_field<std::uint8_t>(character, kCharacterEquipmentActive) == 0) return nullptr;
    return pointer_field(character, kCharacterEquipment);
}

std::uintptr_t call_final_pose_pass(const ArmTargets& t, void* list, std::uint32_t count) noexcept {
    const FinalPosePassFn fn = CodeAddress{t.final_pose_pass};
    return fn(list, count);
}

void* call_bone_world_transform(const ArmTargets& t, void* animation, int bone, int selector,
                                void* out) noexcept {
    const BoneWorldTransformFn fn = CodeAddress{t.bone_world_transform};
    return fn(animation, nullptr, bone, selector, out);
}

std::uintptr_t call_limb_ik(const ArmTargets& t, void* setup, void* second, void* pose,
                            void* fourth) noexcept {
    const LimbIkFn fn = CodeAddress{t.limb_ik};
    return fn(setup, second, pose, fourth);
}

void call_equipment_update(const ArmTargets& t, void* component) noexcept {
    if (!t.equipment_update || !component) return;
    const EquipmentUpdateFn fn = CodeAddress{t.equipment_update};
    fn(component, nullptr);
}

std::optional<Vec3> bone_world_position(const ArmTargets& t, void* animation, int bone,
                                        int selector) noexcept {
    if (!t.bone_world_transform || !animation || bone < 0) return std::nullopt;
    alignas(kTransformAlignment) BoneTransform out{};
    call_bone_world_transform(t, animation, bone, selector, &out);
    return Vec3{out.translation[0], out.translation[1], out.translation[2]};
}

} // namespace omvr::game::hands
