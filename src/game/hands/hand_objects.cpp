// OpenMotherVR-Provenance: clean
#include "game/hands/hand_objects.h"

#include "game/code_address.h"

#include <cstring>

namespace omvr::game::hands {

namespace {

using core::hands::AimRay;
using util::Vec3;

// The game's __thiscall methods, called as __fastcall with an unused EDX
// (STYLEGUIDE 6): `this` in ECX, no stack arguments.
using AimRayUpdateFn = void(__fastcall*)(void* self, void* edx);
using CurrentItemFn  = void*(__fastcall*)(void* self, void* edx);
using TrackerScanFn  = bool(__fastcall*)(void* self, void* edx);

// Above this the game's truncation to a 32-bit integer overflows.
constexpr float kHealthProductLimit = 2147483648.0f;

const std::uint8_t* at(const void* object, std::ptrdiff_t offset) noexcept {
    return static_cast<const std::uint8_t*>(object) + offset;
}
std::uint8_t* at(void* object, std::ptrdiff_t offset) noexcept {
    return static_cast<std::uint8_t*>(object) + offset;
}

template <class T>
T read(const void* object, std::ptrdiff_t offset) noexcept {
    T v;
    std::memcpy(&v, at(object, offset), sizeof(T));
    return v;
}

template <class T>
void write(void* object, std::ptrdiff_t offset, const T& v) noexcept {
    std::memcpy(at(object, offset), &v, sizeof(T));
}

void* pointer_at(const void* object, std::ptrdiff_t offset) noexcept {
    return CodeAddress{read<std::uint32_t>(object, offset)};
}

const void* registry(const HandTargets& t) noexcept {
    return t.registry ? pointer_at(t.registry, 0) : nullptr;
}

Vec3 read_vec3(const void* object, std::ptrdiff_t offset) noexcept {
    return {read<float>(object, offset), read<float>(object, offset + sizeof(float)),
            read<float>(object, offset + 2 * sizeof(float))};
}

void write_vec3(void* object, std::ptrdiff_t offset, const Vec3& v) noexcept {
    write(object, offset, v.x);
    write(object, offset + sizeof(float), v.y);
    write(object, offset + 2 * sizeof(float), v.z);
}

} // namespace

void* player_character(const HandTargets& t) noexcept {
    const void* reg = registry(t);
    if (!reg) return nullptr;
    const void* list = pointer_at(reg, kRegistryCharacterList);
    if (!list || read<std::uint32_t>(list, kListCount) == 0) return nullptr;
    const void* data = pointer_at(list, kListData);
    return data ? pointer_at(data, 0) : nullptr;
}

bool alive(const void* character) noexcept { return read<std::uint16_t>(character, kCharacterHealth) > 0; }

core::hands::Health health(const void* character) noexcept {
    const auto base   = read<std::uint16_t>(character, kCharacterHealthBase);
    const auto factor = read<float>(character, kCharacterHealthMaxFactor);
    // As the game's getter: float product, truncated, low 16 bits. A factor
    // that is not a number or out of range gives no maximum (0) rather than
    // an undefined conversion.
    const float product = static_cast<float>(base) * factor;
    const auto maximum  = product >= 0.0f && product < kHealthProductLimit
                              ? static_cast<std::uint16_t>(static_cast<std::int32_t>(product))
                              : std::uint16_t{0};
    return {address_of(character), read<std::uint16_t>(character, kCharacterHealth), maximum};
}

void* weapon_component(void* character) noexcept {
    void* component = pointer_at(character, kCharacterWeaponComponent);
    if (!component || pointer_at(component, kComponentOwner) != character) return nullptr;
    return component;
}

bool fire_aligned(const void* component) noexcept {
    return read<std::uint8_t>(component, kComponentFireAligned) != 0;
}

std::optional<core::hands::CameraPose> active_camera(const HandTargets& t) noexcept {
    const void* reg = registry(t);
    if (!reg) return std::nullopt;
    const void* manager = pointer_at(reg, kRegistryCameraManager);
    if (!manager) return std::nullopt;
    const void* camera = pointer_at(manager, kManagerActiveCamera);
    if (!camera) return std::nullopt;
    core::hands::CameraPose pose;
    pose.orientation = {read<float>(camera, kCameraOrientation),
                        read<float>(camera, kCameraOrientation + sizeof(float)),
                        read<float>(camera, kCameraOrientation + 2 * sizeof(float)),
                        read<float>(camera, kCameraOrientation + 3 * sizeof(float))};
    pose.position    = read_vec3(camera, kCameraPosition);
    return pose;
}

void call_aim_ray_update(const HandTargets& t, void* component) noexcept {
    const AimRayUpdateFn fn = CodeAddress{t.aim_ray_update};
    fn(component, nullptr);
}

AimRay read_aim_ray(const void* component) noexcept {
    return {read_vec3(component, kAimStart), read_vec3(component, kAimDirection),
            read_vec3(component, kAimEnd)};
}

void write_aim_ray(void* component, const AimRay& ray) noexcept {
    write_vec3(component, kAimStart, ray.start);
    write_vec3(component, kAimDirection, ray.direction);
    write_vec3(component, kAimEnd, ray.end);
    write_vec3(component, kAimEndCopy, ray.end);
}

std::optional<WeaponType> current_weapon_type(const HandTargets& t, void* component) noexcept {
    const CurrentItemFn fn = CodeAddress{t.current_item};
    const void* weapon     = fn(component, nullptr);
    if (!weapon) return std::nullopt;
    const auto type = read<std::int32_t>(weapon, kWeaponType);
    if (type < static_cast<std::int32_t>(WeaponType::NoWeapon) ||
        type > static_cast<std::int32_t>(WeaponType::Cattleprod)) {
        return WeaponType::Unknown;
    }
    return static_cast<WeaponType>(type);
}

bool call_tracker_scan(const HandTargets& t, void* tracker) noexcept {
    const TrackerScanFn fn = CodeAddress{t.tracker_scan};
    return fn(tracker, nullptr);
}

TrackerFacing read_tracker_facing(const void* tracker) noexcept {
    return {read<float>(tracker, kTrackerFacingX), read<float>(tracker, kTrackerFacingZ)};
}

void write_tracker_facing(void* tracker, TrackerFacing f) noexcept {
    write(tracker, kTrackerFacingX, f.x);
    write(tracker, kTrackerFacingZ, f.z);
}

} // namespace omvr::game::hands
