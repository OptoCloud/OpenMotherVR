// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/types.h"
#include "util/math/vec.h"

#include <cstddef>
#include <cstdint>
#include <optional>

// The game objects the arm pose edit reads and writes (spec 21 §2.3,
// §4.2): the animation component's model-to-world transform, the pose's
// local and model-space arrays and flags, and the game functions the edit
// calls. Thin accessors over game memory; no decisions.
namespace omvr::game::hands {

// ---- Layouts (21 §2.3) ------------------------------------------------------------

// 21 §2.3 (r9): the character's model-to-world transform on the animation
// component: translation, rotation quaternion (x, y, z, w) and scale, four
// floats each, 16-byte aligned.
inline constexpr std::ptrdiff_t kAnimationWorldTranslation = 0x270;
inline constexpr std::ptrdiff_t kAnimationWorldRotation    = 0x280;
inline constexpr std::ptrdiff_t kAnimationWorldScale       = 0x290;
// The Havok pose's local-space bone transforms (array data and size) and
// its per-bone flags (data and size); the model-space array is in
// body_objects.h.
inline constexpr std::ptrdiff_t kPoseLocalData  = 0x04;
inline constexpr std::ptrdiff_t kPoseLocalCount = 0x08;
inline constexpr std::ptrdiff_t kPoseFlagsData  = 0x1C;
inline constexpr std::ptrdiff_t kPoseFlagsCount = 0x20;
// The equipment component on the character (21 §2.1; the weapon
// component of hand_objects.h is the same object), and the byte the stage
// tests before it runs the equipment update for a character.
inline constexpr std::ptrdiff_t kCharacterEquipment       = 0x1100;
inline constexpr std::ptrdiff_t kCharacterEquipmentActive = 0x2FA;

// 21 §2.3 (r9): BoneWorldTransform's pose selector of the final pose (entry
// 7 of the component's pose array, the pose FinalPoseStep edits), and the
// one the held item's attachment query passes (6).
inline constexpr int kFinalPoseSelector  = 7;
inline constexpr int kAttachPoseSelector = 6;

// The ABIs of the functions whose calls are redirected (21 §20).
// The final-pose pass: cdecl, the stage's character list and its count.
using FinalPosePassFn = std::uintptr_t(__cdecl*)(void* list, std::uint32_t count);
// BoneWorldTransform: __thiscall (component; bone, selector, out), ret 0xC.
using BoneWorldTransformFn = void*(__fastcall*)(void* self, void* edx, int bone, int selector, void* out);
// The limb IK: stdcall, four arguments (its setup, two the game's, the
// pose third), ret 0x10.
using LimbIkFn = std::uintptr_t(__stdcall*)(void* setup, void* second, void* pose, void* fourth);

// The game functions the edit calls, found by the installer (21 §20) and
// set once before any call is redirected; null: not found or not as
// expected.
struct ArmTargets {
    void* final_pose_pass      = nullptr; // cdecl (character list, count)
    void* equipment_update     = nullptr; // void __thiscall (equipment component)
    void* bone_world_transform = nullptr; // __thiscall (component; bone, selector, out)
    void* limb_ik              = nullptr; // stdcall (four args)
};

// The character's model-to-world transform (21 §2.3). Any thread.
core::hands::ModelToWorld model_to_world(const void* animation) noexcept;

// The pose's local and model-space arrays, flags and its skeleton's
// parents, all of the skeleton's bone count; nullopt when they are missing,
// misaligned or not all of that size. Spans over the game's memory, for
// the job thread that owns the pose (21 §17).
std::optional<core::hands::PoseArrays> pose_arrays(void* pose) noexcept;

// The character's equipment component when the stage runs its equipment
// update (21 §2.1), or null.
void* equipment_component(void* character) noexcept;

// The game's own functions behind the redirected calls.
std::uintptr_t call_final_pose_pass(const ArmTargets& t, void* list, std::uint32_t count) noexcept;
void* call_bone_world_transform(const ArmTargets& t, void* animation, int bone, int selector,
                                void* out) noexcept;
std::uintptr_t call_limb_ik(const ArmTargets& t, void* setup, void* second, void* pose,
                            void* fourth) noexcept;

// The game's equipment update on `component` (21 §2.1). Game thread.
void call_equipment_update(const ArmTargets& t, void* component) noexcept;

// BoneWorldTransform on `animation` for `bone` and `selector`: the bone's
// world translation, or nullopt without the function. Game thread.
std::optional<util::Vec3> bone_world_position(const ArmTargets& t, void* animation, int bone,
                                              int selector) noexcept;

} // namespace omvr::game::hands
