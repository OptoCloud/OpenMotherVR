// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"
#include "game/patcher.h"
#include "game/ui/hud_objects.h"

// The game sites of the HUD options (spec 23 §7, 22 §8.3), each group
// checked as a whole before anything of it is written (12 §4); a group
// whose code differs, or whose addresses this build lacks, stays off and
// is logged. Installed on the game thread when its feature is first wanted
// (with every key at its default nothing here is checked or written).
namespace omvr::game::sites {

// The replacements the sites are pointed at.
struct HudHooks {
    // The scripted HUD entity's hide and show methods (22 §2.4), in their
    // method table: they note the game's own intent, then run the game's.
    ui::HudEntityMethodFn entity_hide;
    ui::HudEntityMethodFn entity_hide_alt;
    ui::HudEntityMethodFn entity_show;
    ui::HudEntityMethodFn entity_show_alt;
    // The call of CrosshairUpdate in the HUD update, and its two invokes.
    ui::CrosshairUpdateFn crosshair_update;
    ui::UiInvokeFn show_crosshairs;
    ui::UiInvokeFn hide_crosshairs;
    // The two calls of the icon highlight update.
    ui::IconHighlightFn icon_highlight;
};

// 22 §8.3, 23 §7.2: the objects' lookup, hide and show and the strings
// they use, checked; the scripted HUD entity's four methods wrapped in
// their six slots. Sets the 22 §2.4 part of `out`.
bool install_hud_visibility_sites(const GameImage& g, Patcher& p, const HudHooks& hooks,
                                  ui::HudTargets& out) noexcept;

// 23 §7.1: CrosshairUpdate's call and its two invokes redirected. Needs the
// lookup part of `out` (its movie invoke).
bool install_crosshair_sites(const GameImage& g, Patcher& p, const HudHooks& hooks,
                             ui::HudTargets& out) noexcept;

// 23 §7.3: the icon highlight update's two calls redirected.
bool install_highlight_sites(const GameImage& g, Patcher& p, const HudHooks& hooks,
                             ui::HudTargets& out) noexcept;

} // namespace omvr::game::sites
