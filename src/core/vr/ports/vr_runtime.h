#pragma once

#include "core/vr/types.h"
#include "util/math/matrix.h"

#include <optional>

// The VR runtime behind the stereo device: one implementation per runtime.
//
// The original has two implementations, MotherVR::OpenVR (vtable 0x1005ddac)
// and MotherVR::Oculus (0x1005dddc), with this interface in this order. Only
// the stereo device calls it, so the vtable need not be binary compatible,
// but the data it hands back must keep the original formats: the 3x4 head
// matrices and the 6-float eye offsets.
//
// Analysis: dumps/06b_hmd_backend.md.

namespace omvr::core::vr::ports {

class VrRuntime {
public:
    virtual ~VrRuntime() = default;

    // Bring up the runtime. False leaves VR off.                        slot 0
    virtual bool init() = 0;
    // Fill the FOV and name fields of `desc`.                           slot 1
    virtual void headset_desc(HeadsetDesc& desc) = 0;
    // Recommended per-eye render size times `pixel_scale`.              slot 2
    virtual TextureSize eye_texture_size(int eye, float pixel_scale) = 0;
    // Called when the game (re)creates its stereo render target. False
    // when the headset cannot be driven at all (no OpenXR session): the
    // stereo device then drops back to no HMD.                          slot 3
    virtual bool create_render_resources(GpuDevice device, GpuContext context) = 0;
    virtual void release_render_resources()                                    = 0; // slot 4
    // Head pose in the game's convention (see to_game_pose), metres
    // times `world_scale`.                                              slot 5
    virtual void head_pose(util::Matrix34& out, float world_scale) = 0;
    // As head_pose, but rotation reduced to yaw: the recenter reference. slot 6
    virtual void head_yaw_pose(util::Matrix34& out, float world_scale) = 0;
    // Eye-to-head offsets: [0..2] left eye, [3..5] right eye, metres.
    // `head_tracked`: the game asked for a head pose since the last
    // submit; when false the frame is shown on a virtual screen.        slot 7
    virtual void begin_frame(float eye_offsets[6], bool head_tracked) = 0;
    // Submit the game's side-by-side eye texture.                       slot 8
    virtual void submit_frame(GpuTexture side_by_side) = 0;
    // Button glyphs and controls help for this controller family.       slot 9
    virtual void register_control_hints() = 0;
    // Read the controllers into the input action tiers, unless the
    // runtime already did at the end of submit_frame.                   slot 10
    virtual void poll_input() = 0;

    // Someone is wearing the headset, upright: a pose taken now is a good
    // recenter reference. (Not in the original, which recenters at launch
    // whatever the headset is doing.)
    virtual bool headset_in_use() { return true; }

    // Whether the last head_yaw_pose took a new reference. False when the
    // head pose was invalid: the previous reference was handed back, and the
    // caller queues a retake (03 §2.1, 09 §5.5). Game thread.
    virtual bool recenter_taken() const { return true; }

    // The runtime moved its tracking space under the mod since the last
    // call (OpenVR: a new chaperone universe or a zero-pose reset, 03 §6.8):
    // the reference is stale, and the caller queues a retake (03 §2.2).
    // True once per change; any pose read after it is in the new space.
    // OpenXR re-anchors its own spaces (09 §6) and never reports one. Game
    // thread.
    virtual bool tracking_space_changed() { return false; }

    // When the FOV only became known after init (OpenXR runtimes that do not
    // report it up front), updates `desc` and returns true once.
    virtual bool refresh_headset_desc(HeadsetDesc& /*desc*/) { return false; }

    // ---- The floor, for standing play (spec 22 §6.1, §6.2) ----
    //
    // Game thread. Heights are runtime metres, before the world scale, +Y up.
    //
    // The runtime has a floor reference (OpenVR's standing universe;
    // OpenXR's STAGE or LOCAL_FLOOR space). Fixed once the runtime runs.
    virtual bool has_floor() const { return false; }
    // The floor's height in the space of the head pose head_pose or
    // head_yaw_pose last returned, located for the same instant. None
    // without a floor reference, or before the floor was first located.
    virtual std::optional<float> floor_height() { return std::nullopt; }
    // The head's height above the floor from the newest valid head pose,
    // read fresh as headset_in_use reads it (menus call no head_pose). None
    // without a floor reference or a valid pose.
    virtual std::optional<float> head_height_above_floor() { return std::nullopt; }

    // The player turned VR off (22 §13): the device has already detached
    // and no new call will come. Stop showing the game in the headset and
    // hand the game released VR controls; an OpenXR session is ended in API
    // order (xrRequestExitSession, xrEndSession at STOPPING) before its
    // instance is destroyed. Render thread, once.
    virtual void stop() {}

    // ---- Controller poses (not in the original; groundwork for the hands) ----
    //
    // `hand` 0 = left, 1 = right. Game thread, like head_pose.
    //
    // The controller's grip pose (OpenXR /input/grip/pose: origin in the
    // palm, -Z forward through the closed fist, +X to the hand's right) in
    // exactly head_pose's space and convention: to_game_pose, same
    // tracking origin and recenter, metres times `world_scale`. Predicted for
    // the same display time as the head pose the game read for the frame it
    // is rendering (or, before any head pose this frame, the newest one), so
    // hands and head line up. False, `out` untouched, when that hand is not
    // tracked.
    virtual bool hand_pose(int /*hand*/, util::Matrix34& /*out*/, float /*world_scale*/) { return false; }

    // The grip's linear velocity at the same instant, in the game's
    // convention (to_game_pose maps a position p to -world_scale * p, so a
    // velocity v to -world_scale * v): game units per second. Both runtimes
    // report it with the pose. False, `out` untouched, when not tracked.
    virtual bool hand_velocity(int /*hand*/, float (& /*out*/)[3], float /*world_scale*/) { return false; }

    // The controller's aim (pointing) pose (OpenXR /input/aim/pose; on
    // OpenVR the pose action bound to the controller's pointing pose,
    // spec 21 §5.2, §16), in hand_pose's space, convention and prediction.
    // Only while the runtime was started with the aim pose actions (Hands
    // Enabled at start, 21 §1). False, `out` untouched, when not tracked.
    virtual bool aim_pose(int /*hand*/, util::Matrix34& /*out*/, float /*world_scale*/) { return false; }
};

} // namespace omvr::core::vr::ports
