// OpenMotherVR-Provenance: clean
#include "core/stereo/play_space.h"

#include <cmath>

namespace omvr::core::stereo {

namespace {

using config::PlayMode;
using diag::ports::logf;

constexpr std::string_view kLogScale = "[stereo] world scale {:.2f}{}";
constexpr std::string_view kLogScaleOverride =
    " (set by -vrscale on the command line; the setting is ignored)";
constexpr std::string_view kLogSeated   = "[stereo] play mode Seated (eye raise {:.2f} m)";
constexpr std::string_view kLogStanding = "[stereo] play mode Standing (standing eye height {:.2f} m)";
constexpr std::string_view kLogStandingUncalibrated =
    "[stereo] play mode Standing, not calibrated: seated height until a calibration";
constexpr std::string_view kLogNoFloor =
    "[stereo] WARNING standing play unavailable on {}: no floor reference; play is seated";

util::Vec3 translation(const util::Matrix34& m) noexcept { return {m.m[0][3], m.m[1][3], m.m[2][3]}; }

util::Vec3 add(const util::Vec3& a, const util::Vec3& b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

void set_translation(util::Matrix34& m, const util::Vec3& t) noexcept {
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
}

// The reference's forward in the horizontal plane: the game looks along +Z
// (21 §2.4), the reference's column 2, which the conversion leaves as the
// runtime gives it. Yaw-only, so already level; normalised in case.
util::Vec3 horizontal_forward(const util::Matrix34& m) noexcept {
    const float x = m.m[0][2], z = m.m[2][2];
    const float n = std::hypot(x, z);
    if (!(n > 0.0f)) return {0.0f, 0.0f, 1.0f};
    return {x / n, 0.0f, z / n};
}

} // namespace

PlaySpaceSettings play_space_settings(const config::Settings& s) noexcept {
    return {.mode                  = s.comfort.play_mode,
            .standing_eye_height_m = s.comfort.standing_eye_height,
            .seated_eye_raise_m    = s.body.seated_eye_raise,
            .eye_forward_m         = s.body.eye_forward,
            .world_scale_setting   = s.comfort.world_scale};
}

float effective_world_scale(float setting, std::optional<float> vrscale) noexcept {
    return vrscale.value_or(setting);
}

void PlaySpace::set_world_scale_switch(std::optional<float> vrscale) noexcept { vrscale_ = vrscale; }

bool PlaySpace::begin_frame(const PlaySpaceSettings& settings) noexcept {
    settings_         = settings;
    const float scale = effective_world_scale(settings.world_scale_setting, vrscale_);
    // At v1's values (scale 1, no switch) the first latch says nothing.
    if (latched_ ? scale != scale_ : (vrscale_ || scale != config::kWorldScaleDefault)) {
        logf(log_, kLogScale, scale, vrscale_ ? kLogScaleOverride : std::string_view{});
    }
    scale_             = scale;
    const bool changed = latched_ && settings.mode != seen_mode_;
    seen_mode_         = settings.mode;
    latched_           = true;
    return changed;
}

util::Matrix34 PlaySpace::on_reference(const util::Matrix34& reference, std::optional<float> floor_y,
                                       std::string_view runtime) noexcept {
    reference_       = translation(reference);
    forward_         = horizontal_forward(reference);
    reference_scale_ = scale_;
    // The convention negates the translation: the height is -y.
    reference_height_ = floor_y ? std::optional<float>{-reference_.y - *floor_y} : std::nullopt;

    // 22 §6.1: the mode applies at a recenter.
    const PlayMode mode = settings_.mode;
    if (mode != applied_mode_ || (!mode_logged_ && mode == PlayMode::Standing)) {
        applied_mode_ = mode;
        mode_logged_  = true;
        log_mode();
    }
    if (mode == PlayMode::Standing && !floor_y && no_floor_logged_.first()) logf(log_, kLogNoFloor, runtime);

    util::Matrix34 out = reference;
    set_translation(out, util::scaled(reference_, scale_));
    return out;
}

void PlaySpace::log_mode() noexcept {
    if (applied_mode_ == PlayMode::Seated) {
        logf(log_, kLogSeated, settings_.seated_eye_raise_m);
    } else if (settings_.standing_eye_height_m > 0.0f) {
        logf(log_, kLogStanding, settings_.standing_eye_height_m);
    } else {
        log_.line(kLogStandingUncalibrated);
    }
}

bool PlaySpace::wants_automatic_calibration() const noexcept {
    return applied_mode_ == PlayMode::Standing && !(settings_.standing_eye_height_m > 0.0f);
}

PlaySpace::Offsets PlaySpace::offsets(std::optional<float> floor_y) const noexcept {
    Offsets o;
    const float h = settings_.standing_eye_height_m;
    if (applied_mode_ == PlayMode::Standing) {
        // 22 §6.1: the game receives (height above the floor - h); the
        // reference holds the height at the recenter, so the head moves by
        // the difference. Not calibrated, or no floor: as seated.
        if (h > 0.0f && floor_y) o.standing.y = *floor_y + h + reference_.y;
    } else {
        // 21 §11.4: raising the eye moves it up, -y in the convention.
        o.body.y = -settings_.seated_eye_raise_m;
    }
    // 21 §11.2: along the reference's forward. The convention negates
    // positions but not the forward axis: moving forward adds +forward.
    o.body = add(o.body, util::scaled(forward_, settings_.eye_forward_m));
    return o;
}

HeadPoses PlaySpace::on_head_pose(const util::Matrix34& head, std::optional<float> floor_y) const noexcept {
    const float s            = scale_;
    const Offsets o          = offsets(floor_y);
    const util::Vec3 real    = util::scaled(translation(head), s);
    const util::Vec3 rescale = util::scaled(reference_, reference_scale_ - s);
    // Exactly the runtime's pose at s when every term is 0 (v1).
    const util::Vec3 player = add(add(real, util::scaled(o.standing, s)), rescale);
    const util::Vec3 game =
        add(add(add(real, util::scaled(o.standing, s)), util::scaled(o.body, s)), rescale);

    HeadPoses p{head, head, head};
    set_translation(p.tracked, real);
    set_translation(p.player, player);
    set_translation(p.game, game);
    return p;
}

} // namespace omvr::core::stereo
