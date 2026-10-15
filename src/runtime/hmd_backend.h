#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;

// The HMD backend behind the stereo device: one per VR runtime.
//
// The original has two implementations, MotherVR::OpenVR (vtable 0x1005ddac)
// and MotherVR::Oculus (0x1005dddc), with this interface in this order. Only
// the stereo device calls it, so the vtable need not be binary compatible,
// but the data it hands back must keep the original formats: the HmdDesc
// layout (it lives inside the game-layout stereo device), the 3x4 head
// matrices and the 6-float eye offsets.
//
// Analysis: dumps/06b_hmd_backend.md.

namespace mothervr::runtime {

// Row-major 3x4, m[r][3] = translation.
struct Matrix34 {
    float m[3][4];
};

struct Sizei {
    std::int32_t w, h;
};

// Tangents of the half-angles, LibOVR ovrFovPort layout.
struct FovPort {
    float up, down, left, right;
};

// LibOVR 0.4.0 ovrHmdDesc. The game's stereo device embeds one at +0x10.
struct HmdDesc {
    void*         handle;              // +0x00
    std::int32_t  type;                // +0x04
    const char*   product_name;        // +0x08  out
    const char*   manufacturer;        // +0x0C  out
    std::uint32_t hmd_caps;            // +0x10
    std::uint32_t sensor_caps;         // +0x14
    std::uint32_t distortion_caps;     // +0x18
    Sizei         resolution;          // +0x1C
    std::int32_t  windows_pos[2];      // +0x24
    FovPort       default_eye_fov[2];  // +0x2C  out: each eye's real FOV
    FovPort       max_eye_fov[2];      // +0x4C  out: the symmetric FOV the game renders
    std::int32_t  eye_render_order[2]; // +0x6C
    const char*   display_device_name; // +0x74
    std::int32_t  display_id;          // +0x78
};
static_assert(sizeof(HmdDesc) == 0x7C);
static_assert(offsetof(HmdDesc, default_eye_fov) == 0x2C);
static_assert(offsetof(HmdDesc, max_eye_fov) == 0x4C);

class HmdBackend {
public:
    virtual ~HmdBackend() = default;

    // Bring up the runtime. False leaves VR off.                        slot 0
    virtual bool init() = 0;
    // Fill the FOV and name fields of `desc`.                           slot 1
    virtual void get_hmd_desc(HmdDesc& desc) = 0;
    // Recommended per-eye render size times `pixel_scale`.              slot 2
    virtual Sizei eye_texture_size(int eye, float pixel_scale) = 0;
    // Called when the game (re)creates its stereo render target. False
    // when the headset cannot be driven at all (no OpenXR session): the
    // stereo device then drops back to no HMD.                          slot 3
    virtual bool create_render_resources(ID3D11Device*, ID3D11DeviceContext*) = 0;
    virtual void release_render_resources() = 0;                      // slot 4
    // Head pose in the game's convention (see to_game_pose), metres
    // times `world_scale`.                                              slot 5
    virtual void head_pose(Matrix34& out, float world_scale) = 0;
    // As head_pose, but rotation reduced to yaw: the recenter reference. slot 6
    virtual void head_yaw_pose(Matrix34& out, float world_scale) = 0;
    // Eye-to-head offsets: [0..2] left eye, [3..5] right eye, metres.
    // `head_tracked`: the game asked for a head pose since the last
    // submit; when false the frame is shown on a virtual screen.        slot 7
    virtual void begin_frame(float eye_offsets[6], bool head_tracked) = 0;
    // Submit the game's side-by-side eye texture.                       slot 8
    virtual void submit_frame(ID3D11Texture2D* side_by_side) = 0;
    // Button glyphs and controls help for this controller family.       slot 9
    virtual void register_control_hints() = 0;
    // Read the controllers into the input action tiers, unless the
    // backend already did at the end of submit_frame.                   slot 10
    virtual void poll_input() = 0;

    // Someone is wearing the headset, upright: a pose taken now is a good
    // recenter reference. (Not in the original, which recenters at launch
    // whatever the headset is doing.)
    virtual bool headset_in_use() { return true; }

    // Whether the last head_yaw_pose took a new reference. False when the
    // head pose was invalid: the previous reference was handed back, and the
    // caller queues a retake (03 §2.1, 09 §5.5). Game thread.
    virtual bool recenter_taken() const { return true; }

    // When the FOV only became known after init (OpenXR runtimes that do not
    // report it up front), updates `desc` and returns true once.
    virtual bool refresh_hmd_desc(HmdDesc& /*desc*/) { return false; }

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
    virtual bool hand_pose(int /*hand*/, Matrix34& /*out*/, float /*world_scale*/) { return false; }

    // The grip's linear velocity at the same instant, in the game's
    // convention (to_game_pose maps a position p to -world_scale * p, so a
    // velocity v to -world_scale * v): game units per second. Both runtimes
    // report it with the pose. False, `out` untouched, when not tracked.
    virtual bool hand_velocity(int /*hand*/, float (& /*out*/)[3], float /*world_scale*/) { return false; }
};

// The virtual screen for menus, videos and loading: the left eye's image
// this far in front of where the head faced at the last recenter, and this
// wide (both metres).
inline constexpr float kCinemaDistance = 1.5f;
inline constexpr float kCinemaWidth    = 32.0f / 9.0f;

// The head pose conversion both original backends apply: negate the X and Y
// basis columns, negate the translation and scale it.
// From: OpenVR slot 5 (FUN_100070e0), the multiply by diag(-1,-1,1,-s).
inline Matrix34 to_game_pose(const float (&r)[3][4], float s) noexcept {
    Matrix34 o{};
    for (int i = 0; i < 3; ++i) {
        o.m[i][0] = -r[i][0];
        o.m[i][1] = -r[i][1];
        o.m[i][2] =  r[i][2];
        o.m[i][3] = -s * r[i][3];
    }
    return o;
}

// The velocity conversion that goes with to_game_pose.
inline void to_game_velocity(const float (&v)[3], float s, float (&out)[3]) noexcept {
    for (int i = 0; i < 3; ++i) out[i] = -s * v[i];
}

// Debug: where `hand` is relative to `head`, in the head's own frame (+X
// right, +Y up, -Z forward), metres. Both are runtime-convention 3x4 poses
// (unconverted, unscaled).
inline void head_local_offset(const float (&head)[3][4], const float (&hand)[3][4], float (&out)[3]) noexcept {
    const float d[3] = {hand[0][3] - head[0][3], hand[1][3] - head[1][3], hand[2][3] - head[2][3]};
    for (int c = 0; c < 3; ++c) out[c] = head[0][c] * d[0] + head[1][c] * d[1] + head[2][c] * d[2];
}

// Every this many frames, with Dev. Debug Options on, the backends log the
// hands' head-local offsets.
inline constexpr std::uint32_t kHandLogFrames = 900;

std::unique_ptr<HmdBackend> make_openvr_backend();
std::unique_ptr<HmdBackend> make_openxr_backend();

} // namespace mothervr::runtime
