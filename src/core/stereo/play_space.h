// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "util/math/matrix.h"
#include "util/math/vec.h"
#include "util/sync/once_latch.h"

#include <optional>
#include <string_view>

// Seated and standing play, the eye's place on the body and the world scale
// (spec 22 §6, 21 §11.1, §11.2, §11.4): what the head pose and the
// reference handed to the game are made of.
//
// The game takes the head pose relative to the reference (02 §6.3). The
// runtime hands both in the game's convention at scale 1 (to_game_pose with
// s = 1: translation -t, runtime metres); this class adds the body offsets
// to the head and applies the world scale s:
//
//     game head = s * head + s * offset + (s0 - s) * reference
//
// where s0 is the scale the reference was handed to the game with. The
// offsets are taken in runtime metres and then scaled (22 §6.3, 21 §11.1).
// The last term keeps the head where it is relative to the reference when
// s changes (22 §6.3: no jump), without rewriting the reference the game
// holds: the game subtracts s0 * reference, so the relative pose is
// s * (head + offset - reference), as with a reference converted with s.
// With every setting at its v1 value, offset and the last term are exactly
// 0 and the head is the runtime's pose at s, as v1 hands it.
//
// Game thread: the stereo update latches the settings (begin_frame), device
// slots 11 and 21 convert the poses.
namespace omvr::core::stereo {

// The settings one game frame uses (22 §6.3: a change applies from the next
// frame whole).
struct PlaySpaceSettings {
    config::PlayMode mode       = config::PlayMode::Seated;
    float standing_eye_height_m = 0.0f; // 0: not calibrated
    float seated_eye_raise_m    = 0.0f;
    float eye_forward_m         = 0.0f;
    float world_scale_setting   = config::kWorldScaleDefault;
};

// The settings of `s` that PlaySpace reads.
PlaySpaceSettings play_space_settings(const config::Settings& s) noexcept;

// 22 §6.3: an accepted -vrscale wins for its run; without one, the setting.
float effective_world_scale(float setting, std::optional<float> vrscale) noexcept;

// What slot 11 hands on, all in the game's convention at this frame's scale.
struct HeadPoses {
    util::Matrix34 game{};    // to the game (slot 11)
    util::Matrix34 tracked{}; // the runtime's pose at this scale, as v1 hands it: the hands' head
    // The game's pose without the body offsets (seated eye raise, eye
    // forward): the player's own displacement from the reference, for the
    // head-in-wall pivot (head_clip_pivot.h).
    util::Matrix34 player{};
};

class PlaySpace final {
public:
    explicit PlaySpace(diag::ports::Log& log) noexcept : log_(log) {}

    // The accepted -vrscale of this run, read once when the device is
    // created (06 §2).
    void set_world_scale_switch(std::optional<float> vrscale) noexcept;

    // Latches this frame's settings. Before any pose of the frame. True when
    // the play mode setting changed since the last call: the caller requests
    // a recenter, at which the new mode applies (22 §6.1). Logs a change of
    // the world scale (22 §16).
    bool begin_frame(const PlaySpaceSettings& settings) noexcept;

    // The scale this frame converts with.
    float world_scale() const noexcept { return scale_; }

    // Slot 21. `reference` is the runtime's yaw-only reference at scale 1,
    // `floor_y` the floor's height in the runtime's space (none without a
    // floor reference), `runtime` the runtime's name for the log. Applies
    // the play mode setting (22 §6.1) and returns the reference for the game.
    util::Matrix34 on_reference(const util::Matrix34& reference, std::optional<float> floor_y,
                                std::string_view runtime) noexcept;

    // Slot 11. `head` is the runtime's head pose at scale 1, `floor_y` as
    // for on_reference.
    HeadPoses on_head_pose(const util::Matrix34& head, std::optional<float> floor_y) const noexcept;

    // The play mode in effect (the setting at the latest recenter).
    config::PlayMode applied_mode() const noexcept { return applied_mode_; }

    // 22 §6.2: standing applies with no calibration yet, so a recenter taken
    // with the headset in use also calibrates.
    bool wants_automatic_calibration() const noexcept;

    // The latest reference's height above the floor, runtime metres; none
    // without a floor reference at that recenter.
    std::optional<float> reference_height() const noexcept { return reference_height_; }

    // 21 §11.5: while the game's crouch was engaged by real crouch, the
    // height the game receives is raised by `game_m` (game metres), so the
    // eye stays at the real head. Game thread; 0 restores v1.
    void set_crouch_raise(float game_m) noexcept { crouch_raise_m_ = game_m; }

private:
    // The offset added to the head before scaling, runtime metres in the
    // game's convention: the standing height (22 §6.1) and, as `body`, the
    // seated eye raise, eye forward and real crouch's raise (21 §11.2,
    // §11.4, §11.5).
    struct Offsets {
        util::Vec3 standing{};
        util::Vec3 body{};
    };
    Offsets offsets(std::optional<float> floor_y) const noexcept;
    void log_mode() noexcept;

    diag::ports::Log& log_;
    PlaySpaceSettings settings_{};
    std::optional<float> vrscale_;
    float scale_                = config::kWorldScaleDefault;
    bool latched_               = false;
    config::PlayMode seen_mode_ = config::PlayMode::Seated; // the setting at the last begin_frame

    // The reference handed to the game: its translation at scale 1, its
    // horizontal forward (the game's +Z, column 2) and the scale it was
    // handed with. The neutral pose before any (03 §1).
    util::Vec3 reference_{};
    util::Vec3 forward_{0.0f, 0.0f, 1.0f};
    float reference_scale_ = config::kWorldScaleDefault;
    std::optional<float> reference_height_;

    config::PlayMode applied_mode_ = config::PlayMode::Seated;
    bool mode_logged_              = false;
    float crouch_raise_m_          = 0.0f; // game metres (21 §11.5)
    util::OnceLatch no_floor_logged_;
};

} // namespace omvr::core::stereo
