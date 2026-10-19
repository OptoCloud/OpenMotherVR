// OpenMotherVR-Provenance: clean
#include "game/ui/hud_sites.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/x86.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace omvr::game::ui {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kLogNoAddress =
    "[hook] {}: off; this build has no addresses for its game sites (23)";
constexpr std::string_view kLogCode = "[hook] {}: off; unexpected code or data at {} (RVA {:08X})";
constexpr std::string_view kLogOn   = "[hook] {}: in place ({} sites)";

constexpr std::string_view kVisibilityArea = "HUD hide and show";
constexpr std::string_view kCrosshairArea  = "crosshair";
constexpr std::string_view kHighlightArea  = "item highlight";

// 22 §2.4: UiFindObject's first instructions, `sub esp, 8; push ebx; push
// ebp; push esi; mov esi, ecx`.
constexpr auto kFindObjectStart =
    x86::concat(x86::sub_esp_imm8(0x08),
                std::array<std::uint8_t, 5>{x86::kPushEbx, x86::kPushEbp, x86::kPushEsi, x86::kMovR32Rm32,
                                            x86::modrm_reg(x86::Reg::Esi, x86::Reg::Ecx)});
static_assert(kFindObjectStart ==
              std::array<std::uint8_t, 8>{0x83, 0xEC, 0x08, 0x53, 0x55, 0x56, 0x8B, 0xF1});
// 22 §2.4: UiInvoke's SEH prologue, `push -1; push <handler>; mov eax,
// fs:[0]`; the handler's address is masked.
constexpr std::uint8_t kSehStateNone = 0xFF; // -1 as a sign-extended imm8
constexpr x86::Pattern<13> kInvokeStart{{x86::kPushImm8, kSehStateNone, x86::kPushImm32, 0, 0, 0, 0,
                                         x86::kFsPrefix, x86::kMovEaxMoffs, 0x00, 0x00, 0x00, 0x00},
                                        {0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
static_assert(kInvokeStart.bytes ==
              std::array<std::uint8_t, 13>{0x6A, 0xFF, 0x68, 0, 0, 0, 0, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00});
// 22 §2.4: WeaponHudHide and Show begin `push esi; mov esi, ecx; cmp dword
// ptr [esi+0x68], 0; je`, then push the function name they invoke.
constexpr std::uint8_t kWeaponHudMovie                = 0x68; // [esi+0x68]
constexpr std::array<std::uint8_t, 7> kWeaponHudStart = {x86::kPushEsi,
                                                         x86::kMovR32Rm32,
                                                         x86::modrm_reg(x86::Reg::Esi, x86::Reg::Ecx),
                                                         x86::kGroup1Imm8Signed,
                                                         x86::modrm_disp8(x86::kGroup1Cmp, x86::Reg::Esi),
                                                         kWeaponHudMovie,
                                                         0x00};
static_assert(kWeaponHudStart == std::array<std::uint8_t, 7>{0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x68, 0x00});
constexpr std::size_t kWeaponHudPush = 9;
// 22 §2.4: PickupHudHide and Show begin `push esi; push <name>`.
constexpr std::size_t kPickupHudPush = 1;
// 23 §2.7: CrosshairUpdate's first instructions, `push ecx; push ebx;
// push esi; push edi; push 0; mov edi, ecx`.
constexpr std::array<std::uint8_t, 8> kCrosshairUpdateStart = {
    x86::kPushEcx,  x86::kPushEbx, x86::kPushEsi,    x86::kPushEdi,
    x86::kPushImm8, 0x00,          x86::kMovR32Rm32, x86::modrm_reg(x86::Reg::Edi, x86::Reg::Ecx)};
static_assert(kCrosshairUpdateStart ==
              std::array<std::uint8_t, 8>{0x51, 0x53, 0x56, 0x57, 0x6A, 0x00, 0x8B, 0xF9});
// 23 §2.6: the icon highlight update's first instructions, `push ebp; mov
// ebp, esp; and esp, -16; push -1`.
constexpr auto kIconHighlightStart =
    x86::concat(x86::kAlignedFramePrologue, std::array<std::uint8_t, 2>{x86::kPushImm8, kSehStateNone});
static_assert(kIconHighlightStart ==
              std::array<std::uint8_t, 8>{0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x6A, 0xFF});

// The scripted HUD entity's method table (22 §2.4): which method each of
// its six slots holds.
enum class EntityMethod : std::uint8_t { Hide, ShowAlt, HideAlt, Show };
constexpr std::array<EntityMethod, 6> kEntitySlots = {EntityMethod::Hide,    EntityMethod::ShowAlt,
                                                      EntityMethod::Hide,    EntityMethod::ShowAlt,
                                                      EntityMethod::HideAlt, EntityMethod::Show};

// A site check: what failed, for the log line.
struct Check {
    const GameImage& g;
    Patcher& p;
    std::string_view area;

    bool fail(std::string_view what, std::uint32_t rva) const noexcept {
        ports::logf(p.log(), kLogCode, area, what, rva);
        return false;
    }
    bool code(std::uint32_t rva, std::string_view what, auto expected) const noexcept {
        return x86::matches(g.at_rva(rva), expected) || fail(what, rva);
    }
    bool string(std::uint32_t rva, std::string_view what, std::string_view text) const noexcept {
        const auto* s = g.at_rva(rva);
        return (std::memcmp(s, text.data(), text.size()) == 0 && s[text.size()] == 0) || fail(what, rva);
    }
    // `push imm32` at `rva` naming the string at `string_rva`.
    bool push_of(std::uint32_t rva, std::uint32_t string_rva, std::string_view what) const noexcept {
        return pushes(g.at_rva(rva), g.at_rva(string_rva)) || fail(what, rva);
    }
    bool call(std::uint32_t site, std::uint32_t target, std::string_view what) const noexcept {
        return calls(g.at_rva(site), g.at_rva(target)) || fail(what, site);
    }
};

bool known(const GameImage& g, Patcher& p, std::string_view area) noexcept {
    if (kHudRvas[g.version].ui_find_object != 0) return true;
    ports::logf(p.log(), kLogNoAddress, area);
    return false;
}

// The lookup and the invoke both groups that call the game need.
bool lookup_expected(const Check& c, const HudRvas& r) noexcept {
    return c.code(r.ui_find_object, "UiFindObject", kFindObjectStart) &&
           c.code(r.ui_invoke, "UiInvoke", kInvokeStart) &&
           c.string(r.weaponstuff_name, "the name weaponstuff", "weaponstuff") &&
           c.string(r.pickup_overlay_name, "the name pickupOverlay", "pickupOverlay");
}

void set_lookup(const GameImage& g, const HudRvas& r, HudTargets& out) noexcept {
    out.ui_manager_ptr = g.at_rva(r.ui_manager_ptr);
    out.ui_ready       = g.at_rva(r.ui_ready);
    out.find_object    = CodeAddress{g.at_rva(r.ui_find_object)};
    out.invoke         = CodeAddress{g.at_rva(r.ui_invoke)};
    out.weaponstuff    = CodeAddress{g.at_rva(r.weaponstuff_name)};
    out.pickup_overlay = CodeAddress{g.at_rva(r.pickup_overlay_name)};
}

std::uint32_t entity_rva(const HudRvas& r, EntityMethod m) noexcept {
    switch (m) {
        case EntityMethod::Hide: return r.hud_entity_hide;
        case EntityMethod::ShowAlt: return r.hud_entity_show_alt;
        case EntityMethod::HideAlt: return r.hud_entity_hide_alt;
        case EntityMethod::Show: return r.hud_entity_show;
    }
    return 0;
}

HudEntityMethodFn entity_hook(const HudHooks& h, EntityMethod m) noexcept {
    switch (m) {
        case EntityMethod::Hide: return h.entity_hide;
        case EntityMethod::ShowAlt: return h.entity_show_alt;
        case EntityMethod::HideAlt: return h.entity_hide_alt;
        case EntityMethod::Show: return h.entity_show;
    }
    return nullptr;
}

bool visibility_expected(const Check& c, const HudRvas& r) noexcept {
    if (!lookup_expected(c, r) ||
        !c.string(r.force_hidden_name, "the name force_hud_hidden", "force_hud_hidden") ||
        !c.string(r.force_visible_name, "the name force_hud_visible", "force_hud_visible") ||
        !c.code(r.weapon_hud_hide, "WeaponHudHide", kWeaponHudStart) ||
        !c.push_of(r.weapon_hud_hide + kWeaponHudPush, r.force_hidden_name, "WeaponHudHide's name") ||
        !c.code(r.weapon_hud_show, "WeaponHudShow", kWeaponHudStart) ||
        !c.push_of(r.weapon_hud_show + kWeaponHudPush, r.force_visible_name, "WeaponHudShow's name") ||
        !c.push_of(r.pickup_hud_hide + kPickupHudPush, r.force_hidden_name, "PickupHudHide") ||
        !c.push_of(r.pickup_hud_show + kPickupHudPush, r.force_visible_name, "PickupHudShow")) {
        return false;
    }
    // PickupHudHide and Show begin with `push esi`.
    if (*c.g.at_rva(r.pickup_hud_hide) != x86::kPushEsi) return c.fail("PickupHudHide", r.pickup_hud_hide);
    if (*c.g.at_rva(r.pickup_hud_show) != x86::kPushEsi) return c.fail("PickupHudShow", r.pickup_hud_show);
    for (std::size_t i = 0; i < kEntitySlots.size(); ++i) {
        const std::uint32_t slot = r.hud_entity_methods + slot_offset(static_cast<std::uint32_t>(i));
        if (!holds_address(c.g.at_rva(slot), c.g.at_rva(entity_rva(r, kEntitySlots[i]))))
            return c.fail("the HUD entity's method table", slot);
    }
    return true;
}

} // namespace

bool install_hud_visibility_sites(const GameImage& g, Patcher& p, const HudHooks& hooks,
                                  HudTargets& out) noexcept {
    if (!known(g, p, kVisibilityArea)) return false;
    const HudRvas& r = kHudRvas[g.version];
    const Check c{g, p, kVisibilityArea};
    if (!visibility_expected(c, r)) return false;
    set_lookup(g, r, out);
    out.weapon_hide     = CodeAddress{g.at_rva(r.weapon_hud_hide)};
    out.weapon_show     = CodeAddress{g.at_rva(r.weapon_hud_show)};
    out.pickup_hide     = CodeAddress{g.at_rva(r.pickup_hud_hide)};
    out.pickup_show     = CodeAddress{g.at_rva(r.pickup_hud_show)};
    out.entity_hide     = CodeAddress{g.at_rva(r.hud_entity_hide)};
    out.entity_hide_alt = CodeAddress{g.at_rva(r.hud_entity_hide_alt)};
    out.entity_show     = CodeAddress{g.at_rva(r.hud_entity_show)};
    out.entity_show_alt = CodeAddress{g.at_rva(r.hud_entity_show_alt)};
    for (std::size_t i = 0; i < kEntitySlots.size(); ++i) {
        std::uint8_t* slot = g.at_rva(r.hud_entity_methods + slot_offset(static_cast<std::uint32_t>(i)));
        if (!p.write_u32(slot, code_address(entity_hook(hooks, kEntitySlots[i])))) {
            // Earlier slots point at wrappers that call the game's own
            // method, so the game behaves as before either way.
            return c.fail("the HUD entity's method table (write)", g.rva_of(slot));
        }
    }
    ports::logf(p.log(), kLogOn, kVisibilityArea, kEntitySlots.size());
    return true;
}

bool install_crosshair_sites(const GameImage& g, Patcher& p, const HudHooks& hooks,
                             HudTargets& out) noexcept {
    if (!known(g, p, kCrosshairArea)) return false;
    const HudRvas& r = kHudRvas[g.version];
    const Check c{g, p, kCrosshairArea};
    const bool expected =
        lookup_expected(c, r) && c.code(r.crosshair_update, "CrosshairUpdate", kCrosshairUpdateStart) &&
        c.call(r.crosshair_update_call, r.crosshair_update, "the call of CrosshairUpdate") &&
        c.string(r.show_crosshairs_name, "the name ShowCrosshairs", "ShowCrosshairs") &&
        c.string(r.hide_crosshairs_name, "the name HideCrosshairs", "HideCrosshairs") &&
        c.push_of(r.show_crosshairs_call - x86::kCallRel32Size, r.show_crosshairs_name,
                  "ShowCrosshairs' push") &&
        c.call(r.show_crosshairs_call, r.ui_invoke, "the invoke of ShowCrosshairs") &&
        c.push_of(r.hide_crosshairs_call - x86::kCallRel32Size, r.hide_crosshairs_name,
                  "HideCrosshairs' push") &&
        c.call(r.hide_crosshairs_call, r.ui_invoke, "the invoke of HideCrosshairs");
    if (!expected) return false;
    set_lookup(g, r, out);
    out.crosshair_update = CodeAddress{g.at_rva(r.crosshair_update)};
    out.hide_crosshairs  = CodeAddress{g.at_rva(r.hide_crosshairs_name)};
    // The invokes first: until the update's call is redirected the game's
    // own update runs them, and the replacements pass them through while
    // the gate has not hidden anything.
    const std::array calls{
        CallRedirect{g.at_rva(r.show_crosshairs_call), code_pointer(hooks.show_crosshairs), "ShowCrosshairs"},
        CallRedirect{g.at_rva(r.hide_crosshairs_call), code_pointer(hooks.hide_crosshairs), "HideCrosshairs"},
        CallRedirect{g.at_rva(r.crosshair_update_call), code_pointer(hooks.crosshair_update),
                     "CrosshairUpdate"}};
    static_assert(calls.size() == 3);
    if (!redirect_all(p, calls)) return false;
    ports::logf(p.log(), kLogOn, kCrosshairArea, calls.size());
    return true;
}

bool install_highlight_sites(const GameImage& g, Patcher& p, const HudHooks& hooks,
                             HudTargets& out) noexcept {
    if (!known(g, p, kHighlightArea)) return false;
    const HudRvas& r = kHudRvas[g.version];
    const Check c{g, p, kHighlightArea};
    const bool expected = c.code(r.icon_highlight, "the icon highlight update", kIconHighlightStart) &&
                          c.call(r.icon_highlight_call_a, r.icon_highlight, "its first call") &&
                          c.call(r.icon_highlight_call_b, r.icon_highlight, "its second call");
    if (!expected) return false;
    out.icon_highlight = CodeAddress{g.at_rva(r.icon_highlight)};
    const std::array calls{
        CallRedirect{g.at_rva(r.icon_highlight_call_a), code_pointer(hooks.icon_highlight), "IconHighlight"},
        CallRedirect{g.at_rva(r.icon_highlight_call_b), code_pointer(hooks.icon_highlight), "IconHighlight"}};
    static_assert(calls.size() == 2);
    if (!redirect_all(p, calls)) return false;
    ports::logf(p.log(), kLogOn, kHighlightArea, calls.size());
    return true;
}

} // namespace omvr::game::ui
