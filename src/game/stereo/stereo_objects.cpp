// The stereo manager and device the game is given in place of its own.
//
// From: FUN_10003510 / FUN_10003640 (init), the StereoInterface vtable
// 0x1005a5c0 and the StereoManager vtable 0x1005a61c.
// Analysis: dumps/06a_stereo_interface.md.

#include "game/stereo/stereo_objects.h"

#include "game/code_address.h"
#include "game/constants.h"
#include "util/math/matrix.h"

#include <algorithm>

namespace omvr::game::stereo {

namespace {

// The vtables the game is handed: static storage, they outlive every use.
DeviceSlots device_vtable{};
ManagerSlots manager_vtable{};

// The game's own device functions for the slots the mod leaves to it.
struct GameDeviceSlots {
    void* eye_transform;
    void* fov_aspect;
    void* eye_scale_offset;
};

// 02 §3.3: the reference pose starts as "no rotation, no translation" in
// the device's head pose convention (03 §1), a 3x4 row-major matrix.
constexpr std::array<float, 12> kNeutralReferencePose = {
    -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
};
// 02 §3.3: position tracking on (+0xA8), at full weight (+0xAC).
constexpr std::uint8_t kPositionTrackingOn = 1;
constexpr float kFullPositionWeight        = 1.0f;
// The device's -vrscale before CreateDevice reads the command line.
constexpr float kDefaultWorldScale = 1.0f;

void* slot(const StereoInterface& si, DeviceSlot index) noexcept {
    return si.vtable[std::to_underlying(index)];
}

void build(StereoManager& m, const DeviceSlots& device, const ManagerSlots& manager,
           const GameDeviceSlots& game) noexcept {
    device_vtable = device;
    // The game's own device code for eye transforms and FOV. It reads our
    // device through the game layout.
    device_vtable[std::to_underlying(DeviceSlot::GameEyeTransform)]   = game.eye_transform;
    device_vtable[std::to_underlying(DeviceSlot::GameFovAspect)]      = game.fov_aspect;
    device_vtable[std::to_underlying(DeviceSlot::GameEyeScaleOffset)] = game.eye_scale_offset;
    manager_vtable                                                    = manager;

    // Manager: the game constructor's values (FUN_10003510), every other
    // field zero as the manager was value-initialised.
    m.vtable = manager_vtable.data();
    util::identity(m.game_matrix);
    m.game_64 = kSceneVisible; // the game constructor's value (02 §3.3)
    m.game_a8 = kPositionTrackingOn;
    m.fade    = kFullPositionWeight;
    std::ranges::copy(kNeutralReferencePose, m.reference_pose);

    // Device (FUN_10003640).
    auto& si       = m.si;
    si.vtable      = device_vtable.data();
    si.world_scale = kDefaultWorldScale;
}

} // namespace

void initialize(const AddressMap& addresses, StereoManager& m, const DeviceSlots& device,
                const ManagerSlots& manager) noexcept {
    build(m, device, manager,
          {addresses.at(AiAddr::GameDeviceEyeTransform), addresses.at(AiAddr::GameDeviceFovAspect),
           addresses.at(AiAddr::GameDeviceEyeScaleOffset)});
}

void describe_as_dk2(StereoInterface& si) noexcept {
    // The handle seed is zero: the game's ovrHmd handle stays null (02 §4).
    si.desc.handle          = CodeAddress{static_cast<std::uint32_t>(si.handle_seed)};
    si.desc.type            = kHmdTypeDk2;
    si.desc.hmd_caps        = kDk2HmdCaps;
    si.desc.sensor_caps     = kDk2SensorCaps;
    si.desc.distortion_caps = kDk2DistortionCaps;
}

void start_sensor(StereoInterface& si) noexcept {
    const abi::NoArgs start = CodeAddress{slot(si, DeviceSlot::StartSensor)};
    start(&si, nullptr);
}

std::int32_t* render_target_size(StereoInterface& si, std::int32_t* w, std::int32_t* h) noexcept {
    const abi::SizeQuery size = CodeAddress{slot(si, DeviceSlot::RenderTargetSize)};
    return size(&si, nullptr, w, h);
}

void recenter(StereoManager& m, StereoInterface& si) noexcept {
    const abi::Pose take = CodeAddress{slot(si, DeviceSlot::RecenterPose)};
    take(&si, nullptr, m.reference_pose);
}

} // namespace omvr::game::stereo
