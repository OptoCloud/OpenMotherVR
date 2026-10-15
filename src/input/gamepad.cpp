// VR input through AI.exe's XInput gamepad.
//
// From: FUN_10008d50 (IsDown), FUN_10008f00 (GetValue), FUN_10008e20
// (GetAxis), FUN_10009030 (ControlToName), FUN_10008fa0 (HasActivity),
// FUN_100031b0 / FUN_10003200 (action-map remap), FUN_10003130 (code
// lookup), FUN_10001090 (glyph names). Analysis: dumps/06d_vr_input.md.

#include "gamepad.h"

#include "diag/log.h"
#include "hook/memory_patch.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <utility>

namespace mothervr::input {

namespace {

// ---- AI.exe action maps (by offset; the game has no symbols) ---------------

struct GameBinding {
    std::int16_t  code;
    std::uint8_t  device;   // 0 pad, 1 keyboard, 2 mouse
    std::uint8_t  flags;
    std::uint32_t unknown;
};
static_assert(sizeof(GameBinding) == 8);

constexpr int kMaxBindings = 6;

struct GameDigital {
    std::uint32_t unknown;
    GameBinding   bindings[kMaxBindings];
    std::int32_t  count;
    std::uint8_t  rest[0x08];
};
static_assert(sizeof(GameDigital) == 0x40);
static_assert(offsetof(GameDigital, count) == 0x34);

struct GameAnalog {
    std::uint32_t unknown;
    GameBinding   bindings[kMaxBindings];
    std::int32_t  count;
    std::uint8_t  rest[0x0C];
};
static_assert(sizeof(GameAnalog) == 0x44);
static_assert(offsetof(GameAnalog, count) == 0x34);

// A binding list the remap blanks out (purpose unknown, see the analysis).
struct GameComposite {
    std::uint32_t codes[7];
    std::int32_t  count;
    std::uint8_t  unknown;
    std::uint8_t  keep;
    std::uint8_t  pad[2];
};
static_assert(sizeof(GameComposite) == 0x24);

struct GameActionMap {
    void*         vftable;
    GameDigital   digital[0x29];        // +0x004
    GameAnalog    analog[0x10];         // +0xA44
    GameComposite composite[12];        // +0xE84 (capacity inferred)
    std::uint8_t  unknown_1034[4];
    std::int32_t  composite_count;      // +0x1038
    void*         device_by_type[3];    // +0x103C  pad, keyboard, mouse
    std::uint8_t  unknown[0x0C];
    std::int32_t  num_digital;          // +0x1054
    std::int32_t  num_analog;           // +0x1058
};
static_assert(offsetof(GameActionMap, analog) == 0xA44);
static_assert(offsetof(GameActionMap, composite) == 0xE84);
static_assert(offsetof(GameActionMap, composite_count) == 0x1038);
static_assert(offsetof(GameActionMap, device_by_type) == 0x103C);
static_assert(offsetof(GameActionMap, num_digital) == 0x1054);
static_assert(offsetof(GameActionMap, num_analog) == 0x1058);

// The input manager, by offset: the three action maps, and the devices the
// stereo update polls (from FUN_1001f8a0). A device's +0x11 byte is the
// game's "active device" flag.
constexpr std::ptrdiff_t kMapOffsets[3]  = {0x48, 0x2998, 0x3A0C};
constexpr std::ptrdiff_t kManagerMouse    = 0x4A70;
constexpr std::ptrdiff_t kManagerKeyboard = 0x4A74;
constexpr std::ptrdiff_t kManagerPad      = 0x4A7C;
constexpr std::ptrdiff_t kDeviceActive    = 0x11;

// ---- The game's pad methods (all __thiscall; see gamepad.h) -----------------

using IsDownFn   = bool        (__fastcall*)(void* self, void* edx, int code);
using ValueFn    = float       (__fastcall*)(void* self, void* edx, int code);
using AxisFn     = float       (__fastcall*)(void* self, void* edx, int code, int negate);
using NameFn     = const char* (__fastcall*)(void* self, void* edx, std::uint32_t code);
using ActivityFn = bool        (__fastcall*)(void* self, void* edx);

struct {
    IsDownFn   is_down  = nullptr;
    ValueFn    value    = nullptr;
    AxisFn     axis     = nullptr;
    NameFn     name     = nullptr;
    ActivityFn activity = nullptr;
} g_game;

void** g_input_manager = nullptr;

// ---- State -----------------------------------------------------------------

std::array<ActionTier, 3> g_tiers = [] {
    std::array<ActionTier, 3> t{};
    for (int i = 0; i < 3; ++i) {
        t[i].base = kTierBase[i];
        t[i].original_code.fill(-1);
    }
    return t;
}();

// Original: DAT_10066bc0 (initially 1), DAT_10068639, DAT_1006af7c.
bool              g_vr_glyphs = true;
std::atomic<bool> g_vr_activity{false};
bool              g_device_pulse = false;

// All five pad slots were replaced (install()). The helpers for the stereo
// update answer from the stock pad alone when they were not.
std::atomic<bool> g_installed{false};

// Original: the std::vector<std::string> at DAT_100686d4, built by
// FUN_10001090. Literals here, so the pointers handed to the game stay valid
// whatever set_glyph does later.
std::array<const char*, 24> g_glyphs = {
    "A", "B", "X", "Y", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
    "BACK", "START", "LB", "RB", "LT_BUTTON", "RT_BUTTON", "LSTICK_BUTTON",
    "RSTICK_BUTTON", "LT", "RT", "LSTICK_X", "LSTICK_Y", "RSTICK_X", "RSTICK_Y",
    "TOUCH_ZOOM_IN", "TOUCH_ZOOM_OUT",
};

std::uint8_t* input_manager() noexcept {
    return g_input_manager ? static_cast<std::uint8_t*>(*g_input_manager) : nullptr;
}

GameActionMap& action_map(std::uint8_t* mgr, int index) noexcept {
    return *reinterpret_cast<GameActionMap*>(mgr + kMapOffsets[index]);
}

void* manager_field(std::ptrdiff_t offset) noexcept {
    std::uint8_t* mgr = input_manager();
    return mgr ? *reinterpret_cast<void**>(mgr + offset) : nullptr;
}

ActionTier& tier_for(int code) noexcept {
    return g_tiers[code < 0x200 ? 0 : code < 0x400 ? 1 : 2];
}

// The pad code a virtual code stood for, -1 for none. The original indexes
// with code & 0xFF unchecked; codes we did not create (a stale map, 0x300..)
// would read past the tier. From: FUN_10003130.
int original_code(int code) noexcept {
    const ActionTier& t = tier_for(code);
    const int k = code - t.base;
    return k >= 0 && k < static_cast<int>(t.original_code.size()) ? t.original_code[k] : -1;
}

// Analog VR value for a virtual code, or 0 when the code is a digital
// action. The original indexes without a bounds check.
float vr_analog(const ActionTier& t, int code) noexcept {
    const int j = (code - t.base) - t.num_digital;
    return j >= 0 && j < t.num_analog ? t.analog[j] : 0.0f;
}

// ---- Remap -----------------------------------------------------------------

// Binds every action of `map` to a virtual code on the pad: the first pad
// binding is rewritten (its pad code kept in the tier), extra pad bindings
// are disabled, and actions without one get one appended. Already-virtual
// bindings are left alone, so this is idempotent.
//
// From: FUN_10003200 (thiscall on the tier, ret 4).
template <class Action>
void remap_actions(ActionTier& t, Action* actions, int count, int index_offset,
                   bool clear_flag_40, std::uint8_t appended_flags) noexcept {
    for (int i = 0; i < count; ++i) {
        Action& act = actions[i];
        const int n = index_offset + i;
        const auto virtual_code = static_cast<std::int16_t>(t.base + n);
        bool found = false;
        for (int b = 0; b < act.count && b < kMaxBindings; ++b) {
            GameBinding& bind = act.bindings[b];
            if (bind.device != 0) continue;
            if (found) {
                bind.code = -1;
            } else {
                found = true;
                if (bind.code < 0x100) {   // signed: -1 counts as not yet virtual
                    t.original_code[n] = bind.code;
                    bind.code = virtual_code;
                }
            }
            if (clear_flag_40) bind.flags &= ~0x40;
        }
        if (!found) {
            // "None" whatever the record count: a full action (six records,
            // none for the pad) must not keep a pad control remembered from
            // before the game rebuilt its bindings (spec 04 section 3.2
            // item 3).
            t.original_code[n] = -1;
            if (act.count < kMaxBindings) { // the original does not bounds-check
                GameBinding& bind = act.bindings[act.count++];
                bind.code   = virtual_code;
                bind.device = 0;
                bind.flags  = appended_flags;
            }
        }
    }
}

void remap(ActionTier& t, GameActionMap& map) noexcept {
    t.num_digital = std::clamp(map.num_digital, 0, 0x29);
    remap_actions(t, map.digital, t.num_digital, 0, true, 0);

    t.num_analog = std::clamp(map.num_analog, 0, 0x10);
    remap_actions(t, map.analog, t.num_analog, t.num_digital, false, 1);

    std::fill(t.original_code.begin() + t.num_digital + t.num_analog,
              t.original_code.end(), std::int16_t{-1});

    // The codes are dwords. The original writes a 16-bit -1 into each,
    // which a 32-bit reader sees as 0x0000FFFF, neither "no control" nor a
    // pad code; -1 in the whole dword holds whichever width the game reads
    // (spec 04 section 3.2 item 4).
    const int composites = std::clamp(map.composite_count, 0, 12);
    for (int c = 0; c < composites; ++c) {
        GameComposite& comp = map.composite[c];
        if (comp.keep) continue;
        for (int i = 0; i < comp.count && i < 7; ++i) {
            comp.codes[i] = 0xFFFFFFFFu;
        }
    }
}

// From: FUN_100031b0.
void remap_all() noexcept {
    std::uint8_t* mgr = input_manager();
    if (!mgr) return;
    for (int i = 0; i < 3; ++i) {
        remap(g_tiers[i], action_map(mgr, i));
    }
    static bool s_logged = false;
    if (!std::exchange(s_logged, true)) {
        diag::logf("[input] action maps virtualised: digital/analog {}/{} {}/{} {}/{}",
                   g_tiers[0].num_digital, g_tiers[0].num_analog,
                   g_tiers[1].num_digital, g_tiers[1].num_analog,
                   g_tiers[2].num_digital, g_tiers[2].num_analog);
    }
}

// ---- The hooks (__thiscall via __fastcall with an unused EDX) ---------------

// bool IsDown(int code). A raw pad code means the maps were (re)loaded and
// need virtualising; the game then sees nothing for it.
// From: FUN_10008d50 (ret 4).
bool __fastcall pad_is_down(void* self, void* /*edx*/, int code) noexcept {
    if (code < 0x100) {
        if (code != -1) remap_all();
        return false;
    }
    const ActionTier& t = tier_for(code);
    const int o = original_code(code);
    bool down = o != -1 && g_game.is_down(self, nullptr, o);
    const int k = code - t.base;
    if (k >= 0 && k < t.num_digital) down |= t.digital[k] != 0;
    return down;
}

// float GetValue(int code): non-negative, the larger of pad and VR.
// From: FUN_10008f00 (ret 4).
float __fastcall pad_value(void* self, void* /*edx*/, int code) noexcept {
    if (code < 0x100) return 0.0f;
    const float game = g_game.value(self, nullptr, original_code(code));
    const float vr   = vr_analog(tier_for(code), code);
    return vr > game ? vr : game;
}

// float GetAxis(int code, bool negate): signed, whichever has the larger
// magnitude. The VR value is not negated: backends write signed values.
// From: FUN_10008e20 (ret 8).
float __fastcall pad_axis(void* self, void* /*edx*/, int code, int negate) noexcept {
    if (code < 0x100) return 0.0f;
    const float game = g_game.axis(self, nullptr, original_code(code), negate);
    const float vr   = vr_analog(tier_for(code), code);
    return std::fabs(vr) > std::fabs(game) ? vr : game;
}

// const char* ControlToName(uint16 code): the glyph id for button prompts.
// From: FUN_10009030 (ret 4; the compare is on the signed 16-bit code).
const char* __fastcall pad_control_name(void* self, void* /*edx*/, std::uint32_t raw) noexcept {
    const auto code = static_cast<std::int16_t>(raw);
    if (code < 0x100) return nullptr;
    const int o = original_code(code);
    if (!g_vr_glyphs) return g_game.name(self, nullptr, static_cast<std::uint16_t>(o));
    if (o < 0 || static_cast<std::size_t>(o) >= g_glyphs.size()) return "VR_MENU"; // original throws past the end
    return g_glyphs[o];
}

// bool HasActivity(): the game uses it to pick the active input device.
// VR activity counts as pad activity. When the source flips between VR and
// the pad, the pad is marked inactive for one call so the game runs its
// device-changed path and refreshes the button prompts.
// From: FUN_10008fa0 (ret 0).
bool __fastcall pad_has_activity(void* self, void* /*edx*/) noexcept {
    const bool game = g_game.activity(self, nullptr);
    const bool vr   = g_vr_activity.exchange(false);
    const bool any  = game || vr;

    std::uint8_t* mgr = input_manager();
    if (!mgr) return any;
    // The player map's pad and keyboard devices (device_by_type[0], [1]).
    void* const* devices = action_map(mgr, 0).device_by_type;
    auto active = [devices](int type) -> std::uint8_t& {
        return static_cast<std::uint8_t*>(devices[type])[kDeviceActive];
    };
    if (any && g_vr_glyphs != vr) {
        g_vr_glyphs    = vr;
        g_device_pulse = true;
        active(0) = 0;
        active(1) = 1;
    } else if (g_device_pulse) {
        g_device_pulse = false;
        active(0) = 1;
        active(1) = 0;
    }
    return any;
}

template <class Fn>
Fn read_slot(const std::uint8_t* slot) noexcept {
    return *reinterpret_cast<Fn const*>(slot);
}

// A failed write is logged by write_dword; that slot keeps the game's function.
bool write_slot(std::uint8_t* slot, const void* fn) noexcept {
    return hook::write_dword(slot, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(fn)));
}

} // namespace

std::array<ActionTier, 3>& tiers() noexcept { return g_tiers; }

void note_vr_activity() noexcept { g_vr_activity.store(true); }

void set_glyph(int pad_code, const char* name) noexcept {
    if (name && pad_code >= 0 && static_cast<std::size_t>(pad_code) < g_glyphs.size()) {
        g_glyphs[pad_code] = name;
    }
}

void* pad_device() noexcept { return manager_field(kManagerPad); }
void* keyboard_device() noexcept { return manager_field(kManagerKeyboard); }
void* mouse_device() noexcept { return manager_field(kManagerMouse); }

bool pad_button(void* pad, int pad_code) noexcept {
    return g_game.is_down && g_game.is_down(pad, nullptr, pad_code);
}

// Without the replacements in place, VR has no effect on the pad: activity
// is the stock pad's own and there is no merged turn value.
bool pad_activity(void* pad) noexcept {
    if (!g_game.activity) return false;
    return g_installed.load() ? pad_has_activity(pad, nullptr) : g_game.activity(pad, nullptr);
}

float pad_axis_value(void* pad, int virtual_code) noexcept {
    return g_game.axis && g_installed.load() ? pad_axis(pad, nullptr, virtual_code, 0) : 0.0f;
}

bool install(std::uint8_t* pad_vtable, void** input_manager) noexcept {
    g_input_manager = input_manager;

    // The original keeps these in DAT_10066c6c..c7c (the same addresses,
    // from its table); reading them from the vtable is equivalent.
    g_game.is_down  = read_slot<IsDownFn>(pad_vtable + 0x00);
    g_game.value    = read_slot<ValueFn>(pad_vtable + 0x04);
    g_game.axis     = read_slot<AxisFn>(pad_vtable + 0x08);
    g_game.name     = read_slot<NameFn>(pad_vtable + 0x10);
    g_game.activity = read_slot<ActivityFn>(pad_vtable + 0x28);

    struct Slot {
        std::ptrdiff_t offset;
        const void*    fn;
    };
    const Slot slots[] = {
        {0x00, reinterpret_cast<const void*>(&pad_is_down)},
        {0x04, reinterpret_cast<const void*>(&pad_value)},
        {0x08, reinterpret_cast<const void*>(&pad_axis)},
        {0x10, reinterpret_cast<const void*>(&pad_control_name)},
        {0x28, reinterpret_cast<const void*>(&pad_has_activity)},
    };
    std::uint32_t saved[std::size(slots)] = {};
    for (std::size_t i = 0; i < std::size(slots); ++i) {
        saved[i] = *reinterpret_cast<const std::uint32_t*>(pad_vtable + slots[i].offset);
    }

    // All or nothing (see gamepad.h): on a failed write, put back the slots
    // already written so the game runs with its stock pad.
    std::size_t written = 0;
    while (written < std::size(slots) &&
           write_slot(pad_vtable + slots[written].offset, slots[written].fn)) {
        ++written;
    }
    if (written < std::size(slots)) {
        bool restored = true;
        for (std::size_t i = 0; i < written; ++i) {
            restored = hook::write_dword(pad_vtable + slots[i].offset, saved[i]) && restored;
        }
        diag::logf("[input] gamepad vtable slot +0x{:X} could not be written; {}: VR controls are off, "
                   "the game keeps its stock pad",
                   slots[written].offset, restored ? "earlier slots restored" : "restoring earlier slots FAILED");
        return false;
    }

    g_installed.store(true);
    diag::logf("[input] gamepad vtable at {} hooked", static_cast<void*>(pad_vtable));
    return true;
}

} // namespace mothervr::input
