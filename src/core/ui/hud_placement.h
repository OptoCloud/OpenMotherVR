// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/stereo/types.h"

#include <atomic>
#include <chrono>
#include <optional>

// The HUD panel in 3D (mode 0, spec 05 §4.3) built from Hud.Placement,
// Hud.Distance, Hud.Width and Hud.Pitch (spec 23 §7.4). With the defaults
// it is v1's panel exactly. The thread that draws the overlay (05 §13).
namespace omvr::core::ui {

using stereo::Matrix44;

// The panel's settings as one frame reads them.
struct HudPanel {
    config::HudPlacement placement = config::HudPlacement::HeadLocked;
    float distance                 = config::kHudDistanceDefault; // metres
    float width                    = config::kHudWidthDefault;    // metres
    float pitch                    = 0.0f;                        // radians, negative below the view

    // v1's panel (05 §4.3).
    [[nodiscard]] bool is_v1() const noexcept;
};

// The settings' values, each read as one word (06 §3.4).
[[nodiscard]] HudPanel hud_panel(const config::HudSettings& hud) noexcept;

// HeadLocked: the panel `p.distance` ahead of the eyes along the view
// direction turned about the head's right axis by `p.pitch`, facing the
// eyes, `p.width` wide; the height follows `inv_aspect`. Identity without a
// view.
[[nodiscard]] Matrix44 head_locked_quad(const std::optional<Matrix44>& view, float inv_aspect,
                                        const HudPanel& p) noexcept;

// Follow's panel: level (no roll), in the horizontal heading `heading`
// (radians about +Y, 0 along +Z), at eye height plus the pitch offset.
[[nodiscard]] Matrix44 level_quad(const std::optional<Matrix44>& view, float heading, float inv_aspect,
                                  const HudPanel& p) noexcept;

// The view's horizontal heading, or nullopt when it looks straight up or
// down (03 §2.1's threshold), where the last heading is kept.
[[nodiscard]] std::optional<float> view_heading(const Matrix44& view) noexcept;

class HudPlacer final {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    // The panel for this frame. `view` is the main view's world-to-view
    // matrix (nullopt before the game has one), `now` a monotonic time.
    Matrix44 place(const std::optional<Matrix44>& view, float inv_aspect, const HudPanel& p,
                   TimePoint now) noexcept;

    // A snap turn or a recenter (05 §3.2c, e): Follow re-places at once.
    // Game thread.
    void request_replace() noexcept { replace_.store(true, std::memory_order_release); }

private:
    struct Move {
        float from = 0.0f, to = 0.0f;
        TimePoint start{};
    };

    // The overlay thread.
    std::optional<float> heading_; // the heading the panel settled on, or is at
    std::optional<Move> move_;
    std::atomic<bool> replace_{false};
};

} // namespace omvr::core::ui
