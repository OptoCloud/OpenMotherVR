#pragma once

#include "core/config/settings.h"
#include "core/ui/ports/game_signals.h"

#include <optional>

namespace omvr::core::ui {

// The settings the game's render targets were last built with. The original
// raises the reset flag on every menu open; a rebuild stalls the game for
// about 0.2 s, so it is raised only when one of these actually changed.
struct TargetSettings {
    float                  pixel_scale;
    config::MirrorViewType mirror_view_type;
    bool                   symmetric_mirror_view;
    bool operator==(const TargetSettings&) const = default;
};

TargetSettings target_settings(const config::Settings& cfg) noexcept;

// Raises the game's display reset when a setting the render targets are
// built with changed, so the change takes effect without a restart; an
// unchanged push leaves it alone (a rebuild is a visible freeze). Game
// thread.
class DisplayReset {
public:
    explicit DisplayReset(ports::GameSignals& signals) noexcept : signals_(signals) {}

    // The first call records what the targets were made with.
    void reset_if_changed(const config::Settings& cfg) noexcept;

private:
    ports::GameSignals&           signals_;
    std::optional<TargetSettings> built_;
};

} // namespace omvr::core::ui
