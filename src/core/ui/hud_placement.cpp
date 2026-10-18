// OpenMotherVR-Provenance: clean
#include "core/ui/hud_placement.h"

#include "core/stereo/tuning.h"
#include "core/stereo/ui_placement.h"
#include "core/ui/hud_tuning.h"
#include "util/math/matrix.h"
#include "util/math/vec.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace omvr::core::ui {

namespace {

using util::Vec3;

// Rows of a quad matrix, as 05 §4 builds them: its right and up axes
// (scaled to the quad's size), its forward axis and its position.
constexpr std::size_t kRightRow    = 0;
constexpr std::size_t kUpRow       = 1;
constexpr std::size_t kForwardRow  = 2;
constexpr std::size_t kPositionRow = 3;

// Columns of the view matrix: the camera's up and forward axes in world
// space (rows 0 to 2).
constexpr std::size_t kViewUpColumn      = 1;
constexpr std::size_t kViewForwardColumn = 2;

constexpr Vec3 kWorldUp{0.0f, 1.0f, 0.0f};
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

Matrix44 identity_quad() noexcept {
    Matrix44 m{};
    util::identity(m.data());
    return m;
}

void set_row(Matrix44& m, std::size_t row, const Vec3& v) noexcept {
    m[row * util::kMatrix4Rows + 0] = v.x;
    m[row * util::kMatrix4Rows + 1] = v.y;
    m[row * util::kMatrix4Rows + 2] = v.z;
}

Vec3 view_axis(const Matrix44& v, std::size_t column) noexcept {
    return {util::element4(v.data(), 0, column), util::element4(v.data(), 1, column),
            util::element4(v.data(), 2, column)};
}

Vec3 add(const Vec3& a, const Vec3& b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

// The panel facing the eyes from `cam`, centred along `forward` turned by
// `pitch` towards `up` (both unit and orthogonal).
Matrix44 panel(const Vec3& cam, const Vec3& forward, const Vec3& up, float inv_aspect,
               const HudPanel& p) noexcept {
    const float c  = std::cos(p.pitch);
    const float s  = std::sin(p.pitch);
    const Vec3 dir = add(util::scaled(forward, c), util::scaled(up, s));
    const Vec3 pup = add(util::scaled(up, c), util::scaled(forward, -s));
    Matrix44 out   = identity_quad();
    // Right = up x forward (left-handed), as v1's panel; the pitch turns
    // both about it, so it stays.
    set_row(out, kRightRow, util::scaled(util::cross(pup, dir), p.width));
    set_row(out, kUpRow, util::scaled(pup, p.width * inv_aspect));
    set_row(out, kForwardRow, dir);
    set_row(out, kPositionRow, add(cam, util::scaled(dir, p.distance)));
    return out;
}

bool finite(const Matrix44& m) noexcept {
    return std::ranges::all_of(m, [](float v) { return std::isfinite(v); });
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
    if (!view) return identity_quad();
    // Defaults: v1's own builder, so v1 is reproduced to the bit.
    if (p.is_v1()) return stereo::hud_panel_quad(view, inv_aspect);
    const Matrix44 out = panel(util::camera_position(view->data()), view_axis(*view, kViewForwardColumn),
                               view_axis(*view, kViewUpColumn), inv_aspect, p);
    return finite(out) ? out : identity_quad();
}

Matrix44 level_quad(const std::optional<Matrix44>& view, float heading, float inv_aspect,
                    const HudPanel& p) noexcept {
    if (!view) return identity_quad();
    const Vec3 forward{std::sin(heading), 0.0f, std::cos(heading)};
    const Matrix44 out = panel(util::camera_position(view->data()), forward, kWorldUp, inv_aspect, p);
    return finite(out) ? out : identity_quad();
}

std::optional<float> view_heading(const Matrix44& view) noexcept {
    const Vec3 f    = view_axis(view, kViewForwardColumn);
    const float len = std::sqrt(f.x * f.x + f.z * f.z);
    if (!(len > stereo::kMinForwardLength)) return std::nullopt;
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
