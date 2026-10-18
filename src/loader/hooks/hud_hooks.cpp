// OpenMotherVR-Provenance: clean
// The interaction and HUD options' hooks (spec 23 §5 to §7, 22 §8.3).

#include "loader/hooks/hud_hooks.h"

#include "core/config/settings_table.h"
#include "core/ui/crosshair_gate.h"
#include "core/ui/hud_placement.h"
#include "game/patcher.h"
#include "game/ui/hud_objects.h"
#include "game/ui/hud_sites.h"
#include "loader/context.h"
#include "loader/hooks/guard.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

namespace gui = game::ui;
using core::diag::ports::logf;
using core::ui::HudAction;
using core::ui::HudMovie;

// 23 §10: once at start-up, each feature's state.
constexpr std::string_view kLogHud =
    "hud: Crosshair {}, Hidden {}, ItemHighlight {}, Placement {} (Distance {:.2f} m, Width {:.2f} m, "
    "Pitch {:.0f} deg), toggle gesture {}";
constexpr std::string_view kLogInteraction =
    "interaction: PhysicalLevers {}, CameraTakeovers {}, TerminalDistance {:.1f} m, RewireDistance {:.1f} m, "
    "TerminalTextScale {:.2f}";
// The features whose game facts are open: their keys are kept, and have no
// effect (23 §12).
constexpr std::string_view kLogLeversOpen =
    "interaction: Levers by Hand has no effect in this build: how the game marks a junction interaction and "
    "what a lever does on release are not known yet (23 §12 items 2 to 4)";
constexpr std::string_view kLogTakeoversOpen =
    "interaction: Camera Takeovers Reduced has no effect in this build: how the game identifies the "
    "animation of a request and the short device animations are not known yet (23 §12 items 5, 6, 12)";
constexpr std::string_view kLogDistanceOpen =
    "interaction: Terminal and Rewire Distance have no effect in this build: the game's activation of these "
    "prompts has not been located (23 §5.2, §12 item 1)";
constexpr std::string_view kLogFastOpen =
    "interaction: FastAnimations is reserved and has no effect (23 §4.6)";
constexpr std::string_view kLogHidden    = "hud: HUD {} ({})";
constexpr std::string_view kLogActions   = "[diag] hud: weaponstuff {}, pickupOverlay {}";
constexpr std::string_view kLogGameHud   = "[diag] hud: the game {} its HUD";
constexpr std::string_view kLogCrosshair = "hud: crosshair {} (Crosshair {})";

// The game image as the installer saw it, for an install once a key asks
// for a group in play; and which groups were tried and went in. Game
// thread (attach runs before the game's threads).
std::optional<game::GameImage> image;
bool visibility_tried = false;
bool crosshair_tried  = false;
bool highlight_tried  = false;
bool crosshair_on     = false;
bool highlight_on     = false;
bool visibility_on    = false;

// The open features' reasons, each logged once when its key first asks.
bool levers_noted    = false;
bool takeovers_noted = false;
bool distance_noted  = false;
bool fast_noted      = false;

// CrosshairUpdate's run: whether the crosshair is to be hidden in it, for
// its ShowCrosshairs invoke; and the last state logged. The HUD update's
// thread.
bool hide_this_update = false;
std::optional<bool> crosshair_logged;

// The last player state logged. Game thread.
std::optional<bool> hidden_logged;

const char* action_name(HudAction a) noexcept {
    switch (a) {
        case HudAction::Hide: return "hidden";
        case HudAction::Show: return "shown";
        case HudAction::None: break;
    }
    return "unchanged";
}

// ---- The scripted HUD entity (22 §2.4): the game's own intent ----------------

// The entity's hide and show methods (two stack arguments, ret 8): the
// game's intent is noted, then its method runs. `kOriginal` is the
// HudTargets slot of the method each instance replaces.
template <bool kShow, gui::HudEntityMethodFn gui::HudTargets::* kOriginal>
int __fastcall entity_method(void* self, void* edx, void* a, void* b) noexcept {
    auto& ctx = context();
    if constexpr (kShow)
        ctx.hud_visibility.on_game_show();
    else
        ctx.hud_visibility.on_game_hide();
    if (ctx.log.debug_enabled()) logf(ctx.log, kLogGameHud, kShow ? "shows" : "hides");
    return (ctx.hud_targets.*kOriginal)(self, edx, a, b);
}

constexpr auto* entity_hide     = &entity_method<false, &gui::HudTargets::entity_hide>;
constexpr auto* entity_hide_alt = &entity_method<false, &gui::HudTargets::entity_hide_alt>;
constexpr auto* entity_show     = &entity_method<true, &gui::HudTargets::entity_show>;
constexpr auto* entity_show_alt = &entity_method<true, &gui::HudTargets::entity_show_alt>;

// ---- The crosshair (23 §7.1) -----------------------------------------------------

bool crosshair_to_hide(Context& ctx) noexcept {
    const auto mode = ctx.settings.current().hud.crosshair;
    if (mode == core::config::CrosshairMode::On) return false;
    return core::ui::crosshair_hidden(mode, ctx.hands.aim_wanted(ctx.clock.now()));
}

// CrosshairUpdate (thiscall on weaponstuff, plain ret), at its one call.
void __fastcall crosshair_update(void* self, void* edx) noexcept {
    auto& ctx = context();
    guarded("CrosshairUpdate", [&] {
        hide_this_update = crosshair_to_hide(ctx);
        if (crosshair_logged != hide_this_update) {
            crosshair_logged = hide_this_update;
            logf(ctx.log, kLogCrosshair, hide_this_update ? "hidden" : "the game's",
                 core::config::to_string(ctx.settings.current().hud.crosshair));
        }
        // The movie invoke dereferences the movie without a check.
        if (!gui::movie_loaded(self)) return;
        const auto before = ctx.crosshair.before_update(hide_this_update, gui::crosshair_cached(self));
        if (before.hide_now) gui::hide_crosshairs(ctx.hud_targets, self);
        if (before.clear_cache) gui::set_crosshair_cached(self, false);
    });
    ctx.hud_targets.crosshair_update(self, edx);
}

// UiInvoke (thiscall (object, name), ret 4) at CrosshairUpdate's
// ShowCrosshairs and HideCrosshairs.
void __fastcall show_crosshairs(void* object, void* edx, const char* function) noexcept {
    auto& ctx = context();
    if (!ctx.crosshair.allow_show(hide_this_update)) return;
    ctx.hud_targets.invoke(object, edx, function);
}

void __fastcall hide_crosshairs(void* object, void* edx, const char* function) noexcept {
    auto& ctx = context();
    if (!ctx.crosshair.allow_hide()) return;
    ctx.hud_targets.invoke(object, edx, function);
}

// ---- Item highlight (23 §7.3) ----------------------------------------------------

// The icon highlight update (thiscall (icon manager, record), ret 4). With
// Hud.ItemHighlight off the record's own "highlight disabled" flag is set
// for the call, so the update finds the icon not highlighted and removes a
// highlight it shows; the flag is then put back as it was.
void __fastcall icon_highlight(void* manager, void* edx, void* record) noexcept {
    auto& ctx = context();
    if (ctx.settings.current().hud.item_highlight || !record) {
        ctx.hud_targets.icon_highlight(manager, edx, record);
        return;
    }
    const bool was_set = gui::set_highlight_disabled(record);
    ctx.hud_targets.icon_highlight(manager, edx, record);
    gui::restore_highlight_disabled(record, was_set);
}

static_assert(std::is_convertible_v<decltype(entity_hide), gui::HudEntityMethodFn>);
static_assert(std::is_convertible_v<decltype(entity_hide_alt), gui::HudEntityMethodFn>);
static_assert(std::is_convertible_v<decltype(entity_show), gui::HudEntityMethodFn>);
static_assert(std::is_convertible_v<decltype(entity_show_alt), gui::HudEntityMethodFn>);
static_assert(std::is_convertible_v<decltype(&crosshair_update), gui::CrosshairUpdateFn>);
static_assert(std::is_convertible_v<decltype(&show_crosshairs), gui::UiInvokeFn>);
static_assert(std::is_convertible_v<decltype(&hide_crosshairs), gui::UiInvokeFn>);
static_assert(std::is_convertible_v<decltype(&icon_highlight), gui::IconHighlightFn>);

constexpr gui::HudHooks kHooks{entity_hide,       entity_hide_alt,  entity_show,      entity_show_alt,
                               &crosshair_update, &show_crosshairs, &hide_crosshairs, &icon_highlight};

// ---- Installing, as the keys ask -----------------------------------------------

void ensure_sites(Context& ctx) noexcept {
    if (!image) return;
    const auto& s = ctx.settings.current();
    game::Patcher patcher{ctx.log};
    if (!visibility_tried && (s.hud.hidden || s.controls.hud_toggle_gesture)) {
        visibility_tried = true;
        visibility_on    = gui::install_hud_visibility_sites(*image, patcher, kHooks, ctx.hud_targets);
        ctx.hud_visibility.set_available(visibility_on);
    }
    if (!crosshair_tried && s.hud.crosshair != core::config::CrosshairMode::On) {
        crosshair_tried = true;
        crosshair_on    = gui::install_crosshair_sites(*image, patcher, kHooks, ctx.hud_targets);
    }
    if (!highlight_tried && !s.hud.item_highlight) {
        highlight_tried = true;
        highlight_on    = gui::install_highlight_sites(*image, patcher, kHooks, ctx.hud_targets);
    }
}

// The open features: a key that asks for one is told why it does nothing,
// once.
void note_open_features(Context& ctx) noexcept {
    const auto& i   = ctx.settings.current().interaction;
    const auto note = [&](bool asks, bool& noted, std::string_view why) {
        if (!asks || noted) return;
        noted = true;
        ctx.log.line(why);
    };
    note(i.physical_levers, levers_noted, kLogLeversOpen);
    note(i.camera_takeovers != core::config::CameraTakeovers::Game, takeovers_noted, kLogTakeoversOpen);
    note(i.terminal_distance != 0.0f || i.rewire_distance != 0.0f, distance_noted, kLogDistanceOpen);
    note(i.fast_animations, fast_noted, kLogFastOpen);
}

void log_state(Context& ctx) noexcept {
    const auto& s = ctx.settings.current();
    logf(ctx.log, kLogHud, core::config::to_string(s.hud.crosshair), s.hud.hidden, s.hud.item_highlight,
         core::config::to_string(s.hud.placement), s.hud.distance, s.hud.width, s.hud.pitch,
         s.controls.hud_toggle_gesture);
    const auto& i = s.interaction;
    logf(ctx.log, kLogInteraction, i.physical_levers, core::config::to_string(i.camera_takeovers),
         i.terminal_distance, i.rewire_distance, i.terminal_text_scale);
}

// 22 §8.3, 23 §7.2: the player's state applied through the game's methods.
void run_visibility(Context& ctx) noexcept {
    if (!visibility_on) return;
    const bool setting = ctx.settings.current().hud.hidden;
    if (ctx.hud_visibility.idle(setting)) return;
    std::array<void*, core::ui::kHudMovieCount> objects{};
    core::ui::HudMovieStates states{};
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const auto which = static_cast<HudMovie>(i);
        objects[i]       = gui::find_hud_object(ctx.hud_targets, which);
        states[i]        = gui::hud_movie_state(which, objects[i]);
    }
    const auto step = ctx.hud_visibility.update(setting, states);
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const auto which = static_cast<HudMovie>(i);
        if (step.actions[i] == HudAction::Hide) gui::hide_hud(ctx.hud_targets, which, objects[i]);
        if (step.actions[i] == HudAction::Show) gui::show_hud(ctx.hud_targets, which, objects[i]);
    }
    if (hidden_logged != step.hidden) {
        hidden_logged = step.hidden;
        logf(ctx.log, kLogHidden, step.hidden ? "hidden" : "shown",
             step.toggled ? "toggle gesture" : "Hud.Hidden");
    }
    if (ctx.log.debug_enabled() && (step.actions[0] != HudAction::None || step.actions[1] != HudAction::None))
        logf(ctx.log, kLogActions, action_name(step.actions[0]), action_name(step.actions[1]));
}

} // namespace

void prepare_hud(const game::GameImage& g) noexcept {
    image.emplace(g);
    auto& ctx = context();
    // 23 §7.2: each session starts as Hud.Hidden says.
    ctx.hud_visibility.start(ctx.settings.current().hud.hidden);
    log_state(ctx);
    note_open_features(ctx);
    ensure_sites(ctx);
}

void hud_on_stereo_update(bool replace) noexcept {
    auto& ctx = context();
    if (replace) ctx.hud_placer.request_replace();
    guarded("hud", [&] {
        ensure_sites(ctx);
        note_open_features(ctx);
        run_visibility(ctx);
    });
}

core::stereo::Matrix44 hud_panel_for_frame(const std::optional<core::stereo::Matrix44>& view,
                                           float inv_aspect) noexcept {
    auto& ctx = context();
    return ctx.hud_placer.place(view, inv_aspect, core::ui::hud_panel(ctx.settings.current().hud),
                                ctx.clock.now());
}

} // namespace omvr::loader::hooks
