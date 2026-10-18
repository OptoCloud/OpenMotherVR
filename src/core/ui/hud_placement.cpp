// OpenMotherVR-Provenance: clean
#include "core/ui/hud_placement.h"

#include "core/stereo/tuning.h"
#include "core/stereo/ui_placement.h"
#include "core/ui/hud_tuning.h"
#include "util/math/finite.h"
#include "util/math/matrix.h"
#include "util/math/vec.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>

namespace omvr::core::ui {

namespace {

using util::add;
using util::column3;
using util::identity44;
using util::kForwardAxis;
using util::kRightAxis;
using util::kTranslationRow;
using util::kUpAxis;
using util::set_row3;
using util::Vec3;

constexpr Vec3 kWorldUp{0.0f, 1.0f, 0.0f};
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

// The panel facing the eyes from `cam`, centred along `forward` turned by
// `pitch` towards `up` (both unit and orthogonal).
Matrix44 panel(const Vec3& cam, const Vec3& forward, const Vec3& up, float inv_aspect,
               const HudPanel& p) noexcept {
    const float c  = std::cos(p.pitch);
    const float s  = std::sin(p.pitch);
    const Vec3 dir = add(util::scaled(forward, c), util::scaled(up, s));
    const Vec3 pup = add(util::scaled(up, c), util::scaled(forward, -s));
    Matrix44 out   = identity44();
    // Right = up x forward (left-handed), as v1's panel; the pitch turns
    // both about it, so it stays.
    set_row3(out, kRightAxis, util::scaled(util::cross(pup, dir), p.width));
    set_row3(out, kUpAxis, util::scaled(pup, p.width * inv_aspect));
    set_row3(out, kForwardAxis, dir);
    set_row3(out, kTranslationRow, add(cam, util::scaled(dir, p.distance)));
    return out;
}

// `a` brought into (-pi, pi].
float wrapped(float a) noexcept {
    a = std::remainder(a, kTwoPi);
    return a <= -std::numbers::pi_v<float> ? a + kTwoPi : a;
}

// Ease-in-out over [0, 1].
float eased(float t) noexcept { return t * t * (3.0f - 2.0f * t); }

} // namespace

bool HudPanel::is_v1() const noexcept {
    return placement == config::HudPlacement::HeadLocked && distance == config::kHudDistanceDefault &&
           width == config::kHudWidthDefault && pitch == 0.0f;
}

HudPanel hud_panel(const config::HudSettings& hud) noexcept {
    return {hud.placement, hud.distance, hud.width, hud.pitch * kRadiansPerDegree};
}

Matrix44 head_locked_quad(const std::optional<Matrix44>& view, float inv_aspect, const HudPanel& p) noexcept {
    if (!view) return identity44();
    // Defaults: v1's own builder, so v1 is reproduced to the bit.
    if (p.is_v1()) return stereo::hud_panel_quad(view, inv_aspect);
    const Matrix44 out = panel(util::camera_position(view->data()), column3(*view, kForwardAxis),
                               column3(*view, kUpAxis), inv_aspect, p);
    return util::all_finite(out) ? out : identity44();
}

Matrix44 level_quad(const std::optional<Matrix44>& view, float heading, float inv_aspect,
                    const HudPanel& p) noexcept {
    if (!view) return identity44();
    const Vec3 forward{std::sin(heading), 0.0f, std::cos(heading)};
    const Matrix44 out = panel(util::camera_position(view->data()), forward, kWorldUp, inv_aspect, p);
    return util::all_finite(out) ? out : identity44();
}

std::optional<float> view_heading(const Matrix44& view) noexcept {
    const Vec3 f    = column3(view, kForwardAxis);
    const float len = std::sqrt(f.x * f.x + f.z * f.z);
    if (!(len > stereo::kMinViewForwardLength)) return std::nullopt;
    return std::atan2(f.x, f.z);
}

Matrix44 HudPlacer::place(const std::optional<Matrix44>& view, float inv_aspect, const HudPanel& p,
                          TimePoint now) noexcept {
    const bool replace = replace_.exchange(false, std::memory_order_acq_rel);
    if (p.placement != config::HudPlacement::Follow || !view) {
        // Re-placed at the head's heading when Follow is chosen again.
        heading_.reset();
        move_.reset();
        return head_locked_quad(view, inv_aspect, p);
    }
    const auto head = view_heading(*view);
    if (head && (!heading_ || replace)) {
        heading_ = *head;
        move_.reset();
    }
    if (!heading_) return level_quad(view, 0.0f, inv_aspect, p);

    if (move_) {
        const float t =
            std::chrono::duration<float>(now - move_->start) / std::chrono::duration<float>(kHudFollowMove);
        if (t >= 1.0f) {
            heading_ = move_->to;
            move_.reset();
        } else {
            heading_ = wrapped(move_->from + eased(std::max(t, 0.0f)) * wrapped(move_->to - move_->from));
        }
    }
    if (!move_ && head && std::abs(wrapped(*head - *heading_)) > kHudFollowThreshold)
        move_ = Move{*heading_, *head, now};
    return level_quad(view, *heading_, inv_aspect, p);
}

} // namespace omvr::core::ui
