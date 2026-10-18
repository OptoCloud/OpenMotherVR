// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/types.h"
#include "game/constants.h"

#include <cstddef>
#include <cstdint>
#include <optional>

// The game objects the hand features read and write (spec 21 §2): the
// player character, the active camera, the player's weapon component and
// the motion tracker. Thin accessors over game memory; no decisions.
// Game thread unless said otherwise.
namespace omvr::game::hands {

// ---- Layouts (21 §2.2, §2.4, §2.5, §2.6) ----------------------------------------

// The character registry: [registry] + 8 is the character list, whose
// count is at +0x84 and data pointer at +0x80; the player is element 0.
inline constexpr std::ptrdiff_t kRegistryCharacterList = 0x08;
inline constexpr std::ptrdiff_t kListData              = 0x80;
inline constexpr std::ptrdiff_t kListCount             = 0x84;
// The character's current health, unsigned 16-bit; its maximum is the
// base (u16) times the factor (float), truncated (the getter 0x37A240).
inline constexpr std::ptrdiff_t kCharacterHealth          = 0x33C;
inline constexpr std::ptrdiff_t kCharacterHealthBase      = 0x33E;
inline constexpr std::ptrdiff_t kCharacterHealthMaxFactor = 0x340;
// The character's weapon component (pointer; 21 §2.2, §2.5): the object
// AimRayUpdate and CurrentItem run on, whose owner is at +0x1528 and whose
// fire alignment byte is at +0x153C.
inline constexpr std::ptrdiff_t kCharacterWeaponComponent = 0x1100;
inline constexpr std::ptrdiff_t kComponentOwner           = 0x1528;
inline constexpr std::ptrdiff_t kComponentFireAligned     = 0x153C;
// The camera manager at [registry] + 0x58; its active camera, the one the
// aim ray is built from, at +0x1DC.
inline constexpr std::ptrdiff_t kRegistryCameraManager = 0x58;
inline constexpr std::ptrdiff_t kManagerActiveCamera   = 0x1DC;
// The camera's orientation quaternion (x, y, z, w) and position (x, y, z).
inline constexpr std::ptrdiff_t kCameraOrientation = 0x2C;
inline constexpr std::ptrdiff_t kCameraPosition    = 0x3C;
// The weapon component's aim ray: end (written twice), direction, start.
inline constexpr std::ptrdiff_t kAimEnd       = 0x50;
inline constexpr std::ptrdiff_t kAimDirection = 0x60;
inline constexpr std::ptrdiff_t kAimStart     = 0x70;
inline constexpr std::ptrdiff_t kAimEndCopy   = 0x90;
// The weapon object's type (the WeaponType enumeration).
inline constexpr std::ptrdiff_t kWeaponType = 0xD4;
// The tracker's horizontal facing the scan reads: x and z.
inline constexpr std::ptrdiff_t kTrackerFacingX = 0xF0;
inline constexpr std::ptrdiff_t kTrackerFacingZ = 0xF8;
// 21 §2.6 (r11): the tracker's byte that is set while it is raised and in
// use [likely]; its caller ANDs the scan's result with it.
inline constexpr std::ptrdiff_t kTrackerRaised = 0x1378;

// The AI.exe functions and globals the hand hooks use, found by the
// installer (21 §20) and set once before any call is redirected.
struct HandTargets {
    const void* registry = nullptr; // the global holding the registry pointer
    void* aim_ray_update = nullptr; // void __thiscall (weapon component)
    void* current_item   = nullptr; // void* __thiscall (weapon component)
    void* tracker_scan   = nullptr; // bool __thiscall (tracker)
};

// The player character, or null before a level has one.
void* player_character(const HandTargets& t) noexcept;

// The player's current health is above 0 (21 §2.2).
bool alive(const void* character) noexcept;

// The character's current and maximum health (21 §2.2).
core::hands::Health health(const void* character) noexcept;

// The character's weapon component, or null when it has none or the
// component names another owner.
void* weapon_component(void* character) noexcept;

// The component's fire alignment byte is set: the game allows firing in
// its aiming states (21 §2.5).
bool fire_aligned(const void* component) noexcept;

// The active camera's pose (21 §2.4), nullopt when there is none.
std::optional<core::hands::CameraPose> active_camera(const HandTargets& t) noexcept;

// The game's own AimRayUpdate on `component`.
void call_aim_ray_update(const HandTargets& t, void* component) noexcept;

// The aim ray AimRayUpdate left in `component`, and writing one back to the
// same fields (both end copies).
core::hands::AimRay read_aim_ray(const void* component) noexcept;
void write_aim_ray(void* component, const core::hands::AimRay& ray) noexcept;

// The weapon type of the component's current item (CurrentItem), nullopt
// when it holds no weapon object.
std::optional<WeaponType> current_weapon_type(const HandTargets& t, void* component) noexcept;

// The game's own TrackerScan on `tracker`. The scan's thread.
bool call_tracker_scan(const HandTargets& t, void* tracker) noexcept;

// The tracker's horizontal facing (x, z) as the scan reads it, and writing
// one.
struct TrackerFacing {
    float x = 0.0f;
    float z = 0.0f;
};
TrackerFacing read_tracker_facing(const void* tracker) noexcept;
void write_tracker_facing(void* tracker, TrackerFacing f) noexcept;

// The tracker's raised byte is set (21 §2.6). The scan's thread.
bool tracker_raised(const void* tracker) noexcept;

} // namespace omvr::game::hands
