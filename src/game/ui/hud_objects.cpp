// OpenMotherVR-Provenance: clean
#include "game/ui/hud_objects.h"

#include "game/code_address.h"
#include "game/field.h"

#include <cstddef>
#include <cstdint>

namespace omvr::game::ui {

namespace {

using core::ui::HudMovie;

// 22 §2.4: the object's movie, null until loaded, and its "HUD forced
// hidden" byte.
constexpr std::ptrdiff_t kMovie        = 0x68;
constexpr std::ptrdiff_t kForcedHidden = 0x40;
// 23 §2.7: CrosshairUpdate's cached decision.
constexpr std::ptrdiff_t kCrosshairCached = 0xE4;
// 23 §2.6: the icon record's flag byte whose bit 3 the highlight update
// reads as "highlight disabled" (the game sets it itself at 0x16B3C3).
constexpr std::ptrdiff_t kIconFlags          = 0x97;
constexpr std::uint8_t kHighlightDisabledBit = 0x08;

} // namespace

void* find_hud_object(const HudTargets& t, HudMovie which) noexcept {
    // The game tests this byte before each lookup of these objects
    // (0x1094AF), and the manager's pointer is null before the UI is up.
    if (!t.find_object || !t.ui_ready || *t.ui_ready == 0) return nullptr;
    void* manager = pointer_at(t.ui_manager_ptr);
    if (!manager) return nullptr;
    return t.find_object(manager, nullptr, which == HudMovie::Weapon ? t.weaponstuff : t.pickup_overlay);
}

core::ui::HudMovieState hud_movie_state(HudMovie which, const void* object) noexcept {
    if (!object) return {};
    // WeaponHudHide and Show test the movie themselves (they set a pending
    // byte without it); PickupHudHide and Show invoke it directly.
    if (which == HudMovie::Pickup && !movie_loaded(object)) return {};
    return {true, *bytes_at(object, kForcedHidden) != 0};
}

void hide_hud(const HudTargets& t, HudMovie which, void* object) noexcept {
    (which == HudMovie::Weapon ? t.weapon_hide : t.pickup_hide)(object, nullptr);
}

void show_hud(const HudTargets& t, HudMovie which, void* object) noexcept {
    (which == HudMovie::Weapon ? t.weapon_show : t.pickup_show)(object, nullptr);
}

bool movie_loaded(const void* object) noexcept { return read_u32(bytes_at(object, kMovie)) != 0; }

bool crosshair_cached(const void* weaponstuff) noexcept {
    return *bytes_at(weaponstuff, kCrosshairCached) != 0;
}

void set_crosshair_cached(void* weaponstuff, bool shown) noexcept {
    *bytes_at(weaponstuff, kCrosshairCached) = shown ? 1 : 0;
}

void hide_crosshairs(const HudTargets& t, void* weaponstuff) noexcept {
    t.invoke(weaponstuff, nullptr, t.hide_crosshairs);
}

bool set_highlight_disabled(void* record) noexcept {
    std::uint8_t& flags = *bytes_at(record, kIconFlags);
    const bool was      = (flags & kHighlightDisabledBit) != 0;
    flags               = static_cast<std::uint8_t>(flags | kHighlightDisabledBit);
    return was;
}

void restore_highlight_disabled(void* record, bool was_set) noexcept {
    if (was_set) return;
    std::uint8_t& flags = *bytes_at(record, kIconFlags);
    flags               = static_cast<std::uint8_t>(flags & ~kHighlightDisabledBit);
}

} // namespace omvr::game::ui
