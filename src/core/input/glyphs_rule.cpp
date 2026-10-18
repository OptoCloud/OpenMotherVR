// OpenMotherVR-Provenance: clean

#include "core/input/glyphs_rule.h"

#include "core/input/glyph_ids.h"
#include "core/input/glyphs.h"
#include "game/constants.h"

#include <array>
#include <cstddef>

namespace omvr::core::input {

namespace {

// SteamVR's controller types and OpenXR's interaction profiles that name a
// family (22 §10.1; public OpenVR and OpenXR documentation).
constexpr std::string_view kTypeIndex = "knuckles";
constexpr std::string_view kTypeTouch = "oculus_touch";
constexpr std::string_view kTypeWmr   = "holographic_controller";
constexpr std::string_view kTypeWand  = "vive_controller";
constexpr std::string_view kTypeFrame = "frame_controller";

constexpr std::string_view kProfileIndex = "/interaction_profiles/valve/index_controller";
constexpr std::string_view kProfileTouch = "/interaction_profiles/oculus/touch_controller";
constexpr std::string_view kProfileWmr   = "/interaction_profiles/microsoft/motion_controller";
constexpr std::string_view kProfileWand  = "/interaction_profiles/htc/vive_controller";
constexpr std::string_view kProfileFrame = "/interaction_profiles/valve/frame_controller_valve";

// The hand whose control a glyph shows. Either: both hands have it (WMR's
// two menu buttons) or the prompt shows the game's own name.
enum class Side { Left, Right, Either };

struct Entry {
    const char* glyph = nullptr; // nullptr: the game's own name
    Side side         = Side::Either;
};

// The stock pad codes 0x00 to 0x0B, which are every code with a VR glyph
// (04 §2.3): A, B, X, Y, the four d-pad directions, BACK, START, LB, RB.
constexpr std::size_t kGlyphCodes = 12;
using Column                      = std::array<Entry, kGlyphCodes>;
static_assert(game::kPadRB + 1 == static_cast<int>(kGlyphCodes));

constexpr Entry L(const char* g) noexcept { return {g, Side::Left}; }
constexpr Entry R(const char* g) noexcept { return {g, Side::Right}; }
constexpr Entry E(const char* g) noexcept { return {g, Side::Either}; }
constexpr Entry kStock{};

// The left stick's four directions: the d-pad of every thumbstick family
// with d-pad emulation on (22 §7, §10.2).
constexpr Entry kStickUp = L(kGlyphLeftStickUp), kStickDown = L(kGlyphLeftStickDown),
                kStickLeft = L(kGlyphLeftStickLeft), kStickRight = L(kGlyphLeftStickRight);

// The columns of 22 §10.2, in pad-code order. The table is a project
// decision per family, from each family's default binding (04 §5.8.2,
// §5.8.4).
constexpr Column kIndex = {
    R(kGlyphAR), R(kGlyphRightStick), R(kGlyphBR), L(kGlyphBL),       kStickUp,          kStickDown,
    kStickLeft,  kStickRight,         L(kGlyphAL), L(kGlyphTrackpad), L(kGlyphLeftGrip), R(kGlyphRightGrip),
};
// Touch, and the generic family, which shows v1's thumbstick set.
constexpr Column kTouch = {
    R(kGlyphA), R(kGlyphRightStick), R(kGlyphB), L(kGlyphY),    kStickUp,          kStickDown,
    kStickLeft, kStickRight,         L(kGlyphX), L(kGlyphMenu), L(kGlyphLeftGrip), R(kGlyphRightGrip),
};
constexpr Column kWmrSteamVr = {
    R(kGlyphTouchpadDownR),
    R(kGlyphRightStick),
    R(kGlyphTouchpadUpR),
    L(kGlyphTouchpadUpL),
    kStickUp,
    kStickDown,
    kStickLeft,
    kStickRight,
    L(kGlyphTouchpadDownL),
    E(kGlyphMenu),
    L(kGlyphLeftGrip),
    R(kGlyphRightGrip),
};
// Under OpenXR the WMR profile has one click per touchpad and no reload or
// flashlight control (04 §5.4, §5.8.4).
constexpr Column kWmrOpenXr = {
    R(kGlyphTouchpadAnyR),
    R(kGlyphRightStick),
    R(kGlyphUnbound),
    kStock,
    kStickUp,
    kStickDown,
    kStickLeft,
    kStickRight,
    L(kGlyphTouchpadAnyL),
    E(kGlyphMenu),
    L(kGlyphLeftGrip),
    R(kGlyphRightGrip),
};
// The wand has no flashlight button (only interact near the head).
constexpr Column kWand = {
    R(kGlyphTouchpadDownR), R(kGlyphTouchpadCentreR), R(kGlyphTouchpadUpR),   kStock,
    L(kGlyphTouchpadUpL),   L(kGlyphTouchpadDownL),   L(kGlyphTouchpadLeftL), L(kGlyphTouchpadRightL),
    L(kGlyphMenuL),         R(kGlyphMenuR),           L(kGlyphLeftGrip),      R(kGlyphRightGrip),
};
// The Steam Frame has a d-pad of its own, which the game's d-pad glyphs
// already show, with or without the emulation.
constexpr Column kFrame = {
    R(kGlyphA),
    R(kGlyphRightStick),
    R(kGlyphB),
    L(kGlyphLeftBumper),
    kStock,
    kStock,
    kStock,
    kStock,
    L(kGlyphView),
    R(kGlyphMenu),
    L(kGlyphLeftGrip),
    R(kGlyphRightGrip),
};

const Column& column(GlyphFamily family, ControllerSource source) noexcept {
    switch (family) {
        case GlyphFamily::Index: return kIndex;
        case GlyphFamily::Wmr: return source == ControllerSource::OpenXr ? kWmrOpenXr : kWmrSteamVr;
        case GlyphFamily::Wand: return kWand;
        case GlyphFamily::Frame: return kFrame;
        case GlyphFamily::Touch:
        case GlyphFamily::Generic:
        case GlyphFamily::Unknown: break;
    }
    return kTouch;
}

bool is_dpad(int code) noexcept { return code >= game::kPadDpadUp && code <= game::kPadDpadRight; }

const char* stock_name(int code) noexcept {
    return code >= 0 && static_cast<std::size_t>(code) < kStockPadNames.size()
               ? kStockPadNames[static_cast<std::size_t>(code)]
               : kGlyphUnbound;
}

const char* glyph_or_stock(const Entry& e, int code) noexcept { return e.glyph ? e.glyph : stock_name(code); }

} // namespace

GlyphFamily glyph_family(ControllerSource source, std::string_view id) noexcept {
    if (source == ControllerSource::OpenVrLegacy) return GlyphFamily::Wand;
    if (id.empty()) return GlyphFamily::Unknown;
    if (source == ControllerSource::OpenXr) {
        if (id == kProfileIndex) return GlyphFamily::Index;
        if (id == kProfileTouch) return GlyphFamily::Touch;
        if (id == kProfileWmr) return GlyphFamily::Wmr;
        if (id == kProfileWand) return GlyphFamily::Wand;
        if (id == kProfileFrame) return GlyphFamily::Frame;
        return GlyphFamily::Generic;
    }
    if (id == kTypeIndex) return GlyphFamily::Index;
    if (id == kTypeTouch) return GlyphFamily::Touch;
    if (id == kTypeWmr) return GlyphFamily::Wmr;
    if (id == kTypeWand) return GlyphFamily::Wand;
    if (id == kTypeFrame) return GlyphFamily::Frame;
    return GlyphFamily::Generic;
}

const char* family_name(GlyphFamily family) noexcept {
    switch (family) {
        case GlyphFamily::Unknown: return "none";
        case GlyphFamily::Index: return "Index";
        case GlyphFamily::Touch: return "Touch";
        case GlyphFamily::Wmr: return "WMR";
        case GlyphFamily::Wand: return "Vive wand";
        case GlyphFamily::Frame: return "Steam Frame";
        case GlyphFamily::Generic: return "generic";
    }
    return "none";
}

const char* family_glyph(GlyphFamily left, GlyphFamily right, ControllerSource source, int code,
                         bool dpad_emulation) noexcept {
    if (source == ControllerSource::OpenVrLegacy) left = right = GlyphFamily::Wand;
    if (left == GlyphFamily::Unknown) left = right;
    if (right == GlyphFamily::Unknown) right = left;
    if (right == GlyphFamily::Unknown) return nullptr;
    if (code < 0 || static_cast<std::size_t>(code) >= kGlyphCodes) return stock_name(code);
    if (is_dpad(code) && !dpad_emulation) return stock_name(code);

    const auto i      = static_cast<std::size_t>(code);
    const Entry& on_l = column(left, source)[i];
    const Entry& on_r = column(right, source)[i];
    // The left controller decides the functions its own binding puts on the
    // left hand, the right one the rest.
    if (on_l.side == Side::Left) return glyph_or_stock(on_l, code);
    return glyph_or_stock(on_r, code);
}

const char* scheme_glyph(ControlScheme scheme, int code, bool dpad_emulation) noexcept {
    const bool touchpad = scheme == ControlScheme::Touchpad;
    if (is_dpad(code)) {
        // A controller's own d-pad (the Frame) shows the game's d-pad.
        if (!dpad_emulation || scheme == ControlScheme::Frame) return stock_name(code);
        return glyph_or_stock((touchpad ? kWand : kTouch)[static_cast<std::size_t>(code)], code);
    }
    switch (code) {
        case game::kPadA: return touchpad ? kGlyphTouchpadDown : kGlyphA;            // interact
        case game::kPadB: return touchpad ? kGlyphTouchpadCentre : kGlyphRightStick; // radial
        case game::kPadX: return touchpad ? kGlyphTouchpadUp : kGlyphB;              // reload
        case game::kPadY: return touchpad ? stock_name(code) : kGlyphY;              // flashlight
        case game::kPadBack: return touchpad ? kGlyphMenu : kGlyphX;                 // map
        case game::kPadStart: return kGlyphMenu;                                     // pause
        case game::kPadLB: return kGlyphLeftGrip;                                    // motion tracker
        case game::kPadRB: return kGlyphRightGrip;                                   // aim
        default: break;
    }
    return stock_name(code);
}

} // namespace omvr::core::input
