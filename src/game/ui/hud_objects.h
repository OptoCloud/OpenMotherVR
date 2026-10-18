// OpenMotherVR-Provenance: clean
#pragma once

#include "core/ui/types.h"

#include <cstdint>

// Thin accessors over the game's HUD objects (spec 22 §2.4, 23 §2.6, §2.7):
// the UI manager's object lookup, the `weaponstuff` and `pickupOverlay`
// objects with their "HUD forced hidden" byte and their hide and show
// methods, CrosshairUpdate's cached decision, and the icon record's
// highlight-disabled flag. Game thread.
namespace omvr::game::ui {

// The game's __thiscall methods, called as __fastcall with an unused EDX
// (STYLEGUIDE 6).
using UiFindObjectFn = void*(__fastcall*)(void* manager, void* edx, const char* name);   // ret 4
using UiInvokeFn     = void(__fastcall*)(void* object, void* edx, const char* function); // ret 4
using HudMethodFn    = void(__fastcall*)(void* object, void* edx);                       // plain ret
// The scripted HUD entity's methods (two stack arguments, ret 8).
using HudEntityMethodFn = int(__fastcall*)(void* self, void* edx, void* a, void* b);
// CrosshairUpdate (thiscall on the weaponstuff object, plain ret) and the
// HUD icon highlight update (thiscall (icon manager, record), ret 4).
using CrosshairUpdateFn = void(__fastcall*)(void* self, void* edx);
using IconHighlightFn   = void(__fastcall*)(void* manager, void* edx, void* record);

// The game's functions and data the HUD options use, set by the installer
// once their checks passed (game/ui/hud_sites.h).
struct HudTargets {
    // 22 §2.4.
    std::uint8_t* ui_manager_ptr = nullptr;
    const std::uint8_t* ui_ready = nullptr;
    UiFindObjectFn find_object   = nullptr;
    UiInvokeFn invoke            = nullptr;
    HudMethodFn weapon_hide      = nullptr;
    HudMethodFn weapon_show      = nullptr;
    HudMethodFn pickup_hide      = nullptr;
    HudMethodFn pickup_show      = nullptr;
    const char* weaponstuff      = nullptr;
    const char* pickup_overlay   = nullptr;
    // The scripted HUD entity's methods behind the slots the mod wraps.
    HudEntityMethodFn entity_hide     = nullptr;
    HudEntityMethodFn entity_hide_alt = nullptr;
    HudEntityMethodFn entity_show     = nullptr;
    HudEntityMethodFn entity_show_alt = nullptr;
    // 23 §2.7.
    CrosshairUpdateFn crosshair_update = nullptr;
    const char* hide_crosshairs        = nullptr;
    // 23 §2.6.
    IconHighlightFn icon_highlight = nullptr;
};

// The UI object of `which` (UiFindObject), or null when it does not exist
// or the game's UI is not up. Takes the UI manager's critical section.
void* find_hud_object(const HudTargets& t, core::ui::HudMovie which) noexcept;

// What the HUD state needs to know of an object found by find_hud_object
// (null: does not exist). For pickupOverlay the movie must be loaded too,
// since its hide and show invoke the movie directly.
core::ui::HudMovieState hud_movie_state(core::ui::HudMovie which, const void* object) noexcept;

// The game's own hide or show method of `object` (22 §2.4).
void hide_hud(const HudTargets& t, core::ui::HudMovie which, void* object) noexcept;
void show_hud(const HudTargets& t, core::ui::HudMovie which, void* object) noexcept;

// The weaponstuff object's movie is loaded (+0x68): the movie invoke
// dereferences it without a check.
bool movie_loaded(const void* object) noexcept;

// CrosshairUpdate's cached decision (+0xE4): the crosshair was last shown.
bool crosshair_cached(const void* weaponstuff) noexcept;
void set_crosshair_cached(void* weaponstuff, bool shown) noexcept;

// Invokes the movie's HideCrosshairs on `weaponstuff`, whose movie is loaded.
void hide_crosshairs(const HudTargets& t, void* weaponstuff) noexcept;

// The icon record's highlight-disabled flag (bit 3 of +0x97): with it set,
// the highlight update finds the icon not highlighted and removes a
// highlight it shows. Set returns the flag's previous value, for restore.
bool set_highlight_disabled(void* record) noexcept;
void restore_highlight_disabled(void* record, bool was_set) noexcept;

} // namespace omvr::game::ui
