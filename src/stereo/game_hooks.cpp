// Game-side hooks around the stereo device. See game_hooks.h.
//
// Analysis: dumps/06c_detours_and_patches.md (sections 3 and 4).

#include "game_hooks.h"

#include "guard.h"

#include "config/config.h"
#include "diag/log.h"
#include "diag/texture_dump.h"
#include "hook/ai_addresses.h"
#include "hook/memory_patch.h"
#include "input/gamepad.h"
#include "integration/script_bridge.h"
#include "stereo_device.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <d3d11.h>
#include <windows.h>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <format>
#include <initializer_list>
#include <utility>

namespace mothervr::stereo {

namespace {

using hook::AiAddr;
using hook::ai_address;

GameHookTargets g_targets{};

// 1e-5, the game's own constant, bit for bit.
constexpr float kHeadClipOn  = std::bit_cast<float>(0x3727C5ACu);
constexpr float kHeadClipOff = 1.0f;
float g_head_clip_threshold = kHeadClipOn;

// The active first-person camera (FollowBehavior), null while none is
// active: the object may be freed once deactivated (a level unload).
// Original: DAT_1006863c, DAT_10068638, which keep the last one.
std::uint8_t* g_follow = nullptr;
bool          g_follow_active = false;
constexpr std::ptrdiff_t kFollowYaw         = 0x204;  // ±1 = ±180°
constexpr std::ptrdiff_t kFollowYawMirror   = 0x1FC;
constexpr std::ptrdiff_t kFollowLookEnabled = 0x264;  // game stick/mouse look
constexpr std::ptrdiff_t kFollowRootPos     = 0x1C0;  // character root, world x y z
constexpr std::ptrdiff_t kFollowHeadPos     = 0x1CC;  // "Head1" bone, world x y z

// A cutscene or level fade is running; the camera left the player during
// it. Original: DAT_1006863a, DAT_1006863b.
bool g_cutscene_or_fade = false;
bool g_cutscene_took_camera = false;

// Update state. Original: StereoManager +0x310..+0x320.
struct {
    bool loading_blackout = false;  // +0x310
    bool keyboard_mouse   = false;  // +0x311  K&M in use: allow game look
    bool r3_latch         = false;  // +0x313
    bool snap_latch       = false;  // +0x314
    std::chrono::steady_clock::time_point snap_time{};  // +0x318 (clock() ms there)
    bool recentered       = false;  // +0x31C
    bool recenter_request = false;  // +0x31D  runtime asked (Oculus), or us:
                                    // gameplay starting, headset put on
    int  screen_replaced  = 0;      // +0x320
} g_update;

// The turn is the player map's analog action 3; its virtual code follows
// the map's digital count (0x12C on the shipped map), so it is derived.
constexpr int   kTurnMap = 0, kTurnAction = 3;
constexpr float kTurnThreshold   = 0.5f;
constexpr auto  kSnapRepeat      = std::chrono::milliseconds(300);
// Debug: how long after a snap step the camera is traced.
constexpr auto  kSnapTraceTime   = std::chrono::milliseconds(600);
std::chrono::steady_clock::time_point g_last_snap{};  // kept past the stick's release
constexpr int   kPadLB = 10, kPadRB = 11, kPadRStick = 15;
constexpr int   kScanLCtrl = 0x1D, kScanLAlt = 0x38;

// ---- Game calls ------------------------------------------------------------

template <class Fn>
Fn game_fn(AiAddr slot) noexcept { return reinterpret_cast<Fn>(ai_address(slot)); }

// Debug: each change of the mode (0 world, 1 virtual screen), to follow
// which menus, videos and loading screens get the screen.
void log_mode_change(int before, int after) noexcept {
    if (before != after && config::current().debug.debug) diag::logf("[diag] mode {} -> {}", before, after);
}

void game_push_mode(StereoManager* sm, int mode) noexcept {
    const int before = sm->mode;
    game_fn<void (__fastcall*)(StereoManager*, void*, int)>(AiAddr::GamePushMode)(sm, nullptr, mode);
    log_mode_change(before, sm->mode);
}
void game_pop_mode(StereoManager* sm) noexcept {
    const int before = sm->mode;
    game_fn<void (__fastcall*)(StereoManager*, void*)>(AiAddr::GamePopMode)(sm, nullptr);
    log_mode_change(before, sm->mode);
}

// Leave mode 1 for the world. The game's pop does nothing with an empty
// stack, which is the state when the game entered mode 1 without a push and
// there was no storage to raise the count into (sync_mode_stack): push 0
// through the game instead, which runs its SetMode(0) and appends with its
// own allocator, then drop that entry so the stack is empty in mode 0.
void game_pop_to_world(StereoManager* sm) noexcept {
    game_pop_mode(sm);
    if (sm->mode == 1 && sm->stack_top == 0) {
        game_push_mode(sm, 0);
        sm->stack_top = 0;
    }
}

// Rule f: at the end of each update the stack count equals the mode, after
// unbalanced game pushes and pops. Lowering writes the count. Raising does
// not, because when the game entered mode 1 without a push the array may
// have no storage (data null, capacity 0) and a count of 1 over it makes
// the game's next pop read element 0 through null. The game's append
// routine, which allocates, is not in the address tables; until it is, the
// count is raised only into storage the game already has, else left as it
// is (game_pop_to_world still gets back to mode 0) and that is logged once.
void sync_mode_stack(StereoManager& m) noexcept {
    if (m.mode >= 0 && m.stack_top > m.mode) {
        m.stack_top = m.mode;
    } else if (m.mode == 1 && m.stack_top == 0) {
        if (m.stack_data && m.stack_capacity >= 1) {
            m.stack_data[0] = 1;
            m.stack_top = 1;
        } else {
            static bool s_logged = false;
            if (!std::exchange(s_logged, true)) {
                diag::log_line("[stereo] mode 1 entered without a push and the mode stack has no storage; "
                               "count left at 0");
            }
        }
    }
}

template <class T = void>
T* field_ptr(std::uint8_t* base, std::ptrdiff_t off) noexcept {
    return *reinterpret_cast<T**>(base + off);
}

template <class T>
T& field(void* base, std::ptrdiff_t off) noexcept {
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(base) + off);
}

// The main view's world-to-view matrix (row vectors, left-handed, metres):
// columns 0..2 are the camera's right, up and forward axes in world space.
const float* view_matrix() noexcept {
    auto* pov = *reinterpret_cast<std::uint8_t**>(ai_address(AiAddr::PovManagerPtr));
    return pov ? field_ptr<float>(pov, 8) : nullptr;
}

struct Vec3 {
    float x, y, z;
};

Vec3 camera_position(const float* v) noexcept {
    return {-(v[12] * v[0] + v[13] * v[1] + v[14] * v[2]),
            -(v[12] * v[4] + v[13] * v[5] + v[14] * v[6]),
            -(v[12] * v[8] + v[13] * v[9] + v[14] * v[10])};
}

void identity(float* m) noexcept {
    for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

StereoManager& sm() noexcept { return manager(); }

// Debug: the camera around a snap step, to tell the body catching up with
// the new heading (the eye follows the head bone, which swings around the
// root as the body turns) from game look turning as well. Game thread.
void trace_snap(std::chrono::steady_clock::time_point step, bool keyboard_mouse) noexcept {
    if (!config::current().debug.debug || step == std::chrono::steady_clock::time_point{}) return;
    const auto since = std::chrono::steady_clock::now() - step;
    if (since > kSnapTraceTime) return;
    const float dx = field<float>(g_follow, kFollowHeadPos) - field<float>(g_follow, kFollowRootPos);
    const float dz = field<float>(g_follow, kFollowHeadPos + 8) - field<float>(g_follow, kFollowRootPos + 8);
    diag::logf("[diag] snap +{}ms: yaw {:.3f} target {:.3f}, head-root {:.3f} m ({:.3f}, {:.3f}), look {}, k&m {}",
               std::chrono::duration_cast<std::chrono::milliseconds>(since).count(),
               field<float>(g_follow, kFollowYawMirror), field<float>(g_follow, kFollowYaw),
               std::hypot(dx, dz), dx, dz, field<std::uint8_t>(g_follow, kFollowLookEnabled),
               keyboard_mouse);
}

} // namespace

void install_game_hooks(const GameHookTargets& targets) noexcept {
    g_targets = targets;
    set_clipping_blinders(config::current().rendering.clipping_blinders);
}

float& head_clip_threshold() noexcept { return g_head_clip_threshold; }

void request_recenter() noexcept { g_update.recenter_request = true; }

void set_clipping_blinders(bool on) noexcept {
    g_head_clip_threshold = on ? kHeadClipOn : kHeadClipOff;
}

// ---- Stereo::Update --------------------------------------------------------
//
// Snap turn on the turn axis, recenter on LB+RB or Ctrl+Alt, game look only
// when snap is off or keyboard/mouse is used, scene visibility for
// cutscenes and loading. The game's own update (DK2 recenter, fade) is not
// run. From: FUN_1001f8a0.

namespace {

void update_running(StereoManager* self, StereoInterface* device) {
    auto& u = g_update;
    const auto& input_cfg = config::current().input;
    void* pad      = input::pad_device();
    void* keyboard = input::keyboard_device();
    void* mouse    = input::mouse_device();

    bool lb = false, rb = false, r3 = false, ctrl = false, alt = false;
    if (pad) {
        lb = input::pad_button(pad, kPadLB);
        rb = input::pad_button(pad, kPadRB);
        r3 = input::pad_button(pad, kPadRStick);
        if (input::pad_activity(pad)) u.keyboard_mouse = false;

        const float turn = input::pad_axis_value(pad, input::analog_virtual_code(kTurnMap, kTurnAction));
        const bool  out  = turn < -kTurnThreshold || turn > kTurnThreshold;
        if (g_follow_active && input_cfg.snap_rotation && out && self->mode != 1 && !u.snap_latch) {
            // One step; yaw is in half turns, wrapped to [-1, 1].
            u.snap_time = g_last_snap = std::chrono::steady_clock::now();
            // A full turn is 2 half turns; in floating point, so 3 segments
            // step 120 degrees rather than 180.
            const float step = 2.0f / static_cast<float>(std::max(input_cfg.snap_rotation_segments, 2));
            float yaw = field<float>(g_follow, kFollowYaw) + (turn < -kTurnThreshold ? -step : step);
            if (yaw > 1.0f) yaw -= 2.0f;
            if (yaw < -1.0f) yaw += 2.0f;
            field<float>(g_follow, kFollowYaw) = yaw;
            field<float>(g_follow, kFollowYawMirror) = yaw;
            u.snap_latch = true;
        } else if ((!out && u.snap_latch) ||
                   (g_follow_active && out && u.snap_latch &&
                    std::chrono::steady_clock::now() - u.snap_time > kSnapRepeat)) {
            // Stick released, or held long enough to repeat.
            u.snap_latch = false;
            u.snap_time = {};
        }
    }
    if (keyboard && mouse) {
        using KeyDownFn = bool (__fastcall*)(void*, void*, int);
        using AnyFn     = bool (__fastcall*)(void*, void*);
        auto key_down = game_fn<KeyDownFn>(AiAddr::KeyboardKeyDown);
        ctrl = key_down(keyboard, nullptr, kScanLCtrl);
        alt  = key_down(keyboard, nullptr, kScanLAlt);
        if (game_fn<AnyFn>(AiAddr::KeyboardAnyKey)(keyboard, nullptr) ||
            game_fn<AnyFn>(AiAddr::MouseActive)(mouse, nullptr)) {
            u.keyboard_mouse = true;
        }
    }
    if (g_follow_active) {
        field<std::uint8_t>(g_follow, kFollowLookEnabled) = !input_cfg.snap_rotation || u.keyboard_mouse;
        trace_snap(g_last_snap, u.keyboard_mouse);
    }

    // Recenter on the chord (or our own request), then re-place the menu
    // screen for two frames. Requests wait until someone is wearing the
    // headset upright.
    using RecenterFn = float* (__fastcall*)(void*, void*, float*);
    auto recenter = [&] {
        reinterpret_cast<RecenterFn>(device->vtable[21])(device, nullptr, self->reference_pose);
    };
    const bool chord = (lb && rb) || (ctrl && alt);
    bool handled = false;
    runtime::HmdBackend* backend = load_backend(*device);
    if (u.recenter_request && backend && backend->headset_in_use()) {
        recenter();
        u.recenter_request = false;
        u.recentered = true;
        u.screen_replaced = 0;
        handled = true;
    } else if (chord) {
        if (!u.recentered) {
            recenter();
            u.recentered = true;
            handled = true;
        } else if (r3 && !u.r3_latch) {
            u.r3_latch = true;
            handled = true;
        }
    }
    if (!handled) {
        if (u.recentered && u.screen_replaced <= 1) {
            place_virtual_screen(self->game_matrix, -1.0f, 0.5625f, -1.0f);
            ++u.screen_replaced;
        } else if (!chord) {
            u.recentered = false;
            u.r3_latch = false;
            u.screen_replaced = 0;
        }
    }

    sync_mode_stack(*self);

    // Scene visibility (+0x64): never black during cutscenes and fades,
    // black while a loading screen is up.
    if (g_cutscene_or_fade) {
        if (self->mode != 1) self->game_64 = 1.0f;
        if (!g_follow_active) {
            g_cutscene_took_camera = true;
        } else if (g_cutscene_took_camera) {
            g_cutscene_or_fade = g_cutscene_took_camera = false;
        }
    }
    if (u.loading_blackout) self->game_64 = 0.0f;
}

} // namespace

int __fastcall stereo_update(StereoManager* self, void*, float /*dt*/) noexcept {
    StereoInterface* device = load_device(*self);
    if (!device || load_state(*device) != DeviceState::Running) {
        if (g_follow_active) field<std::uint8_t>(g_follow, kFollowLookEnabled) = 1;
        return 0;
    }
    guarded("Stereo::Update", [&] { update_running(self, device); });
    return 0;
}

// ---- UI quad placement -------------------------------------------------------

// The menu screen: level, facing where the camera looks, 1.5 m ahead at eye
// height. The game's own placement follows the full head orientation.
// From: FUN_1001fc20 (yaw from the view through a quaternion; same result).
float* __cdecl place_virtual_screen(float* out, float, float inv_aspect, float) noexcept {
    const float* v = view_matrix();
    identity(out);
    if (!v) return out;
    float fx = v[2], fz = v[10];
    const float len = std::sqrt(fx * fx + fz * fz);
    if (len > 1e-6f) {
        fx /= len;
        fz /= len;
    } else {
        fx = 0.0f;
        fz = 1.0f;
    }
    // Rows: right = up x forward (left-handed), up scaled to the aspect,
    // forward, position.
    out[0] = fz;   out[1] = 0.0f;       out[2]  = -fx;
    out[4] = 0.0f; out[5] = inv_aspect; out[6]  = 0.0f;
    out[8] = fx;   out[9] = 0.0f;       out[10] = fz;
    const Vec3 cam = camera_position(v);
    out[12] = cam.x + fx * 1.5f;
    out[13] = cam.y;
    out[14] = cam.z + fz * 1.5f;
    return out;
}

// The HUD in 3D: a panel 0.55 m wide, 1 m in front of the eyes, locked to
// the head. From: FUN_1001ffd0.
float* __cdecl place_hud_panel(float* out, float, float inv_aspect, float) noexcept {
    const float* v = view_matrix();
    identity(out);
    if (!v) return out;
    constexpr float kWidth = 0.55f;
    const Vec3 up{v[1], v[5], v[9]};
    const Vec3 fwd{v[2], v[6], v[10]};
    out[0] = kWidth * (up.y * fwd.z - up.z * fwd.y);
    out[1] = kWidth * (up.z * fwd.x - up.x * fwd.z);
    out[2] = kWidth * (up.x * fwd.y - up.y * fwd.x);
    out[4] = up.x * kWidth * inv_aspect;
    out[5] = up.y * kWidth * inv_aspect;
    out[6] = up.z * kWidth * inv_aspect;
    out[8] = fwd.x;
    out[9] = fwd.y;
    out[10] = fwd.z;
    const Vec3 cam = camera_position(v);
    out[12] = fwd.x + cam.x;
    out[13] = fwd.y + cam.y;
    out[14] = fwd.z + cam.z;
    return out;
}

// ---- Mode stack ------------------------------------------------------------------
// Mode 0: 3D world with the HUD panel. Mode 1: the virtual screen (menus,
// videos). The stack is kept one deep, so each push re-places the screen in
// front of the current view. From: FUN_100201e0, FUN_10020220.

void __fastcall push_mode(StereoManager* self, void*, int mode) noexcept {
    if (mode == 1 && self->mode == 0) {
        game_push_mode(self, 1);
        return;
    }
    game_pop_mode(self);
    game_push_mode(self, mode);
}

void __fastcall pop_mode(StereoManager* self, void*) noexcept {
    if (self->mode == 1) game_pop_to_world(self);
}

// The control prompts ask whether movement is on the left stick; the game's
// own check reads the remapped bindings. Before the first virtualisation
// the move action has no recorded control, which the stock check answers
// true. From: FUN_10003100.
bool __fastcall is_move_on_left_stick(void*, void*) noexcept {
    return input::move_on_left_stick();
}

// ---- Planar reflections -------------------------------------------------------------

namespace {

ReflectionTargets g_reflection{};
bool g_reflection_ready = false;

// Read by the clear gate's machine code.
volatile std::uint8_t g_skip_clears = 0;

// POV fields.
constexpr std::ptrdiff_t kPovView     = 0x000;  // float[16]
constexpr std::ptrdiff_t kPovProj     = 0x040;  // float[16]
constexpr std::ptrdiff_t kPovProjCopy = 0x080;  // float[16], used for shader constants
constexpr std::ptrdiff_t kPovFov      = 0x0E0;  // vertical FOV, radians
constexpr std::ptrdiff_t kPovAspect   = 0x0E4;
constexpr std::ptrdiff_t kPovFlags    = 0x36B;  // bit 0 active, bit 4 renders
constexpr std::ptrdiff_t kPovEye      = 0x378;  // -1 none; selects the viewport half

// Puts a gate in front of the game's clear: while g_skip_clears is set it
// returns at once (the clear is __cdecl, so the caller pops the arguments).
// The clear's first six bytes are a position-independent prologue, which
// the gate runs before jumping back.
bool install_clear_gate(std::uint8_t* clear) noexcept {
    // push ebp; mov ebp,esp; sub esp,0x10
    constexpr std::uint8_t kPrologue[6] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10};
    if (std::memcmp(clear, kPrologue, sizeof(kPrologue)) != 0) {
        diag::log_line("[stereo] clear gate: unexpected code, reflections stay mono");
        return false;
    }
    auto* stub = static_cast<std::uint8_t*>(
        ::VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!stub) return false;
    std::uint8_t* p = stub;
    auto put = [&p](std::initializer_list<std::uint8_t> bytes) {
        for (auto b : bytes) *p++ = b;
    };
    auto put32 = [&p](std::uint32_t v) {
        std::memcpy(p, &v, 4);
        p += 4;
    };
    const auto addr = [](const volatile void* a) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(a));
    };
    put({0x80, 0x3D}); put32(addr(&g_skip_clears)); put({0x00});  // cmp byte ptr [g_skip_clears], 0
    put({0x75, 0x0B});                                             // jne skip
    put({0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10});                     // the clear's prologue
    put({0xE9}); put32(addr(clear + 6) - addr(p + 4));             // jmp clear+6
    put({0xC3});                                                   // skip: ret
    DWORD old_protect = 0;
    ::VirtualProtect(stub, 64, PAGE_EXECUTE_READ, &old_protect);   // written once; no W+X
    ::FlushInstructionCache(::GetCurrentProcess(), stub, 64);

    std::uint8_t entry[6] = {0xE9, 0, 0, 0, 0, 0x90};              // jmp stub; nop
    const std::uint32_t rel = addr(stub) - addr(clear + 5);
    std::memcpy(entry + 1, &rel, 4);
    return hook::write_bytes(clear, entry, sizeof(entry));
}

// Eye `eye`'s transform on the POV's centre view and projection, and the
// derived fields: what POV::SetEye does, without its lighting and
// reflection-plane side effects.
void setup_eye(std::uint8_t* pov, int eye) noexcept {
    using ApplyFn   = void (__fastcall*)(void*, void*, int, float*, float*);
    using FrustumFn = void (__fastcall*)(void*, void*);
    field<std::int32_t>(pov, kPovEye) = eye;
    reinterpret_cast<ApplyFn>(g_reflection.apply_eye_transform)(
        &sm(), nullptr, eye, &field<float>(pov, kPovView), &field<float>(pov, kPovProj));
    reinterpret_cast<FrustumFn>(g_reflection.update_frustum)(pov, nullptr);
    const float* proj = &field<float>(pov, kPovProj);
    field<float>(pov, kPovAspect) = proj[5] / proj[0];
    field<float>(pov, kPovFov) = 2.0f * std::atan(1.0f / proj[5]);
    std::memcpy(pov + kPovProjCopy, proj, 0x40);
    if (auto* owner = static_cast<std::uint8_t*>(*g_reflection.renderer_eye_owner)) {
        field<std::int32_t>(owner, 0x2C) = eye;
    }
}

// Debug (F10): write what the reflection pass left bound as render target
// to <game dir>/mothervr_reflection_<pass>.dds.
void dump_bound_target(int pass) {
    void* renderer = *reinterpret_cast<void**>(ai_address(AiAddr::RendererPtr));
    if (!renderer) return;
    auto* ctx = game_fn<ID3D11DeviceContext* (__cdecl*)(void*)>(AiAddr::RendererContext)(renderer);
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) {
        diag::logf("[diag] reflection pass {}: no render target bound", pass);
        return;
    }
    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    diag::dump_texture(ctx, res, std::format("mothervr_reflection_{}.dds", pass));
    res->Release();
    rtv->Release();
}

} // namespace

bool install_reflection_fix(const ReflectionTargets& targets) noexcept {
    g_reflection = targets;
    g_reflection_ready = install_clear_gate(targets.clear);
    return g_reflection_ready;
}

void __fastcall render_reflection(void* pov, void*) noexcept {
    using RenderFn = void (__fastcall*)(void*, void*);
    auto render = reinterpret_cast<RenderFn>(g_reflection.render);
    auto* p = static_cast<std::uint8_t*>(pov);

    StereoInterface* dev = load_device(sm());
    const DeviceState state = dev ? load_state(*dev) : DeviceState::None;
    {   // Diagnostics: each reflection POV and every change of its flags.
        static std::array<std::pair<const void*, int>, 8> s_seen{};
        static int s_lines = 0;
        auto it = std::find_if(s_seen.begin(), s_seen.end(),
                               [&](const auto& e) { return e.first == pov || e.first == nullptr; });
        if (it != s_seen.end() && (it->first == nullptr || it->second != p[kPovFlags]) && s_lines < 60) {
            ++s_lines;
            diag::logf("[stereo] reflection POV {} flags 0x{:02X} -> 0x{:02X} (state {}, size {}x{})",
                       pov, it->first ? it->second : -1, p[kPovFlags],
                       dev ? static_cast<int>(state) : -1,
                       field<std::int32_t>(p, 0x33C), field<std::int32_t>(p, 0x340));
            *it = {pov, p[kPovFlags]};
        }
    }
    if (!g_reflection_ready || !dev || state != DeviceState::Running ||
        (p[kPovFlags] & 0x11) != 0x11) {
        render(pov, nullptr);   // flat, or nothing to render: as the game does
        return;
    }

    // The centre (mono) state, restored for each eye and afterwards.
    std::array<std::uint8_t, 0xC0> centre;   // view, proj, proj copy
    std::memcpy(centre.data(), p, centre.size());
    const float saved_fov = field<float>(p, kPovFov);
    const float saved_aspect = field<float>(p, kPovAspect);
    const std::int32_t saved_eye = field<std::int32_t>(p, kPovEye);
    auto* owner = static_cast<std::uint8_t*>(*g_reflection.renderer_eye_owner);
    const std::int32_t saved_renderer_eye = owner ? field<std::int32_t>(owner, 0x2C) : 0;

    // The capture is a DebugOptions diagnostic: with it off the key does
    // nothing.
    static bool s_dump_key = false;
    const bool dump_key = config::current().debug.debug && (::GetAsyncKeyState(VK_F10) & 0x8000);
    const bool dump = dump_key && !s_dump_key;
    s_dump_key = dump_key;

    for (int eye = 0; eye < 2; ++eye) {
        std::memcpy(p, centre.data(), 0x80);
        setup_eye(p, eye);
        g_skip_clears = eye == 1;   // keep the first half
        render(pov, nullptr);
        g_skip_clears = 0;
        if (dump) guarded("reflection capture", [&] { dump_bound_target(eye); });
    }

    std::memcpy(p, centre.data(), centre.size());
    field<float>(p, kPovFov) = saved_fov;
    field<float>(p, kPovAspect) = saved_aspect;
    field<std::int32_t>(p, kPovEye) = saved_eye;
    if (owner) field<std::int32_t>(owner, 0x2C) = saved_renderer_eye;

    static bool s_logged = false;
    if (!std::exchange(s_logged, true)) diag::log_line("[stereo] reflections rendered per eye");
}

// ---- Anti-aliasing in stereo --------------------------------------------------------

namespace {

StereoAaTargets g_aa{};

// PostFX fields.
constexpr std::ptrdiff_t kPostFxMeshDraw = 0x1B2F;  // AA pass draws with its own mesh instead of
                                                    // the full-screen triangle
constexpr std::ptrdiff_t kPostFxAaMode   = 0x1B3C;  // setting 9

enum AaMode : std::int32_t { kAaOff = 0, kFxaa = 1, kSmaaT1x = 2, kSmaaT2x = 3 };

// Also called from the game's job threads (the cull hooks), hence the
// atomic loads; only the main thread changes the device and its state.
bool stereo_running() noexcept {
    StereoInterface* dev = load_device(sm());
    return dev && load_state(*dev) == DeviceState::Running;
}

// The first stereo frame with each AA mode, and each change of what the
// mod does with it: run it, run T2x as T1x, or skip it (setting off, or
// Auto above its pixel scale). Render thread: once per change only.
void log_stereo_aa(std::int32_t mode, bool run) noexcept {
    static std::int32_t s_mode = -1;
    static int s_action = -1;
    const config::StereoAA setting = config::current().rendering.stereo_aa;
    const int action = !run ? 0 : mode == kSmaaT2x ? 2 : 1;
    if (mode == s_mode && action == s_action) return;
    s_mode   = mode;
    s_action = action;
    constexpr const char* kModes[]   = {"off", "FXAA", "SMAA T1x", "SMAA T2x"};
    constexpr const char* kActions[] = {"skipped", "runs it", "runs it as SMAA T1x"};
    const char* name = mode >= kAaOff && mode <= kSmaaT2x ? kModes[mode] : "unknown";
    diag::logf("[stereo] Stereo anti-aliasing for game mode {}: {} (setting {})", name,
               mode == kAaOff ? "nothing to run" : kActions[action], config::to_string(setting));
}

} // namespace

void install_stereo_aa(const StereoAaTargets& targets) noexcept { g_aa = targets; }

void __cdecl stereo_final_draw() noexcept {
    using DrawFn   = void (__cdecl*)();
    using PostFxFn = void* (__cdecl*)();
    using AaFn     = void (__fastcall*)(void*, void*);
    void* postfx = stereo_running() ? reinterpret_cast<PostFxFn>(g_aa.postfx)() : nullptr;
    const std::int32_t mode = postfx ? field<std::int32_t>(postfx, kPostFxAaMode) : kAaOff;
    // StereoAntiAliasing: off, or Auto above the pixel scale where the eyes
    // are already supersampled (about 0.6 ms GPU for little gain).
    const bool run = mode >= kFxaa && mode <= kSmaaT2x && config::stereo_aa_enabled(config::current().rendering);
    if (postfx) log_stereo_aa(mode, run);
    if (!run) {
        reinterpret_cast<DrawFn>(g_aa.fullscreen_draw)();   // as the game does
        return;
    }
    // The AA pass draws the bound effect (postprocess_stereo_final, fade
    // already in c36) into its intermediate, then filters into the output.
    // The stereo path always draws with the full-screen triangle; keep that.
    auto& mesh_draw = field<std::uint8_t>(postfx, kPostFxMeshDraw);
    const std::uint8_t saved_mesh_draw = std::exchange(mesh_draw, std::uint8_t{0});
    reinterpret_cast<AaFn>(g_aa.aa_pass)(postfx, nullptr);
    mesh_draw = saved_mesh_draw;
}

void __cdecl run_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept {
    using SmaaFn = void (__cdecl*)(void*, void*, const std::uint8_t*);
    // Spatial SMAA in stereo; it also zeroes the projection jitter that
    // the temporal mode would apply next frame.
    const std::uint8_t spatial = 0;
    if (*temporal && stereo_running()) temporal = &spatial;
    reinterpret_cast<SmaaFn>(g_aa.smaa)(color, second, temporal);
}

// ---- Object culling -----------------------------------------------------------------

namespace {

using CullFn = bool (__cdecl*)(const void*, const void*, const void*);
using DistanceFn = bool (__cdecl*)(const void*, const float*, float);

CullTargets g_cull{};

} // namespace

void install_cull_fix(const CullTargets& targets) noexcept { g_cull = targets; }

// Called from the game's job threads; see stereo_running().
bool __cdecl cull_distance(const void* bounds, const float* camera, float scale) noexcept {
    return stereo_running() || reinterpret_cast<DistanceFn>(g_cull.distance)(bounds, camera, scale);
}

// Only reached while the main POV culls: `buffer` is the context's
// occlusion buffer, which no other POV has (see the header).
bool __cdecl cull_occlusion(const void* bounds, const void* ctx, const void* buffer) noexcept {
    return stereo_running() || reinterpret_cast<CullFn>(g_cull.occlusion)(bounds, ctx, buffer);
}

// ---- Head-in-wall blackout delay ----------------------------------------------------

namespace {

using HeadClipFadeFn = float (__stdcall*)(const float*, const float*);

HeadClipFadeFn g_head_clip_fade = nullptr;

constexpr float kFadeClear = 0.99999f;
constexpr auto  kFadeGrace = std::chrono::milliseconds(600);

// The last call with the view clear. Camera update, main thread only.
std::chrono::steady_clock::time_point g_head_clear_at{};

} // namespace

void install_head_clip_delay(void* head_clip_fade) noexcept {
    g_head_clip_fade = reinterpret_cast<HeadClipFadeFn>(head_clip_fade);
}

float __stdcall head_clip_fade(const float* eye, const float* pivot) noexcept {
    const float fade = g_head_clip_fade(eye, pivot);
    if (!stereo_running()) return fade;
    const auto now = std::chrono::steady_clock::now();
    if (fade >= kFadeClear) {
        g_head_clear_at = now;
        return fade;
    }
    return now - g_head_clear_at < kFadeGrace ? 1.0f : fade;
}

// ---- Vtable replacements ---------------------------------------------------------

namespace {

using FollowFn = int (__fastcall*)(void*, void*, void*);
using MenuFn   = void (__fastcall*)(void*, void*);

void call_menu(void* fn, void* self) noexcept {
    if (fn) reinterpret_cast<MenuFn>(fn)(self, nullptr);
}

// Show the virtual screen while a menu is up.
void enter_screen_mode() noexcept {
    if (sm().mode != 0) game_pop_mode(&sm());
    game_push_mode(&sm(), 1);
}

void leave_screen_mode() noexcept {
    if (sm().mode == 1) game_pop_to_world(&sm());
}

} // namespace

// From: FUN_10001720 / FUN_10001740.
int __fastcall follow_on_activate(void* self, void*, void* arg) noexcept {
    g_follow = static_cast<std::uint8_t*>(self);
    g_follow_active = true;
    return reinterpret_cast<FollowFn>(g_targets.follow_on_activate)(self, nullptr, arg);
}

// Unlike the original, forget the camera once it is deactivated: the
// stereo update writes through g_follow every frame, and the object may be
// freed after this.
int __fastcall follow_on_deactivate(void* self, void*, void* arg) noexcept {
    g_follow_active = false;
    const int r = reinterpret_cast<FollowFn>(g_targets.follow_on_deactivate)(self, nullptr, arg);
    if (g_follow == self) g_follow = nullptr;
    return r;
}

// Watch for cutscene and level-fade start/finish messages.
// From: FUN_1001f830 (naked in the original, reading the caller's frame;
// we read our own first argument).
int __fastcall ui_on_message(void* self, void*, const std::uint32_t* msg, int a, int b, int c) noexcept {
    const std::uint32_t id = msg ? *msg : 0;
    auto name = [](AiAddr slot) { return *reinterpret_cast<const std::uint32_t*>(ai_address(slot)); };
    if (id == name(AiAddr::CutsceneVisibilityStart) || id == name(AiAddr::LevelFadeStart)) {
        g_cutscene_or_fade = true;
    } else if (id == name(AiAddr::CutsceneVisibilityFinish) || id == name(AiAddr::LevelFadeFinish)) {
        g_cutscene_or_fade = false;
    }
    using Fn = int (__fastcall*)(void*, void*, const std::uint32_t*, int, int, int);
    return reinterpret_cast<Fn>(g_targets.ui_on_message)(self, nullptr, msg, a, b, c);
}

// From: FUN_10020c80.
void __fastcall menu_close(void* self, void*) noexcept {
    call_menu(g_targets.menu_close, self);
    leave_screen_mode();
}

// From: FUN_10020ca0.
void __fastcall pause_open(void* self, void*) noexcept {
    call_menu(g_targets.pause_open, self);
    enter_screen_mode();
    integration::push_settings_to_game(self);
}

// From: FUN_10020cf0.
void __fastcall pause_close(void* self, void*) noexcept {
    call_menu(g_targets.pause_close, self);
    leave_screen_mode();
}

// From: FUN_10020d10. The 3D scene goes black behind the loading screen.
void __fastcall loading_open(void* self, void*) noexcept {
    call_menu(g_targets.loading_open, self);
    enter_screen_mode();
    g_update.loading_blackout = true;
    integration::push_settings_to_game(self);
}

// From: FUN_10020d60.
void __fastcall loading_close(void* self, void*) noexcept {
    call_menu(g_targets.loading_close, self);
    g_update.loading_blackout = false;
    leave_screen_mode();
    // Entering gameplay: forward is where the player faces now.
    request_recenter();
}

// From: FUN_10020da0.
void __fastcall attract_open(void* self, void*) noexcept {
    call_menu(g_targets.attract_open, self);
    enter_screen_mode();
}

// From: FUN_10020dd0 (the game's close is an empty method).
void __fastcall attract_close(void*, void*) noexcept {
    leave_screen_mode();
}

} // namespace mothervr::stereo
