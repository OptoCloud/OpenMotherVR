// OpenMotherVR-Provenance: clean
// The haptics' hooks (spec 21 §14). Each reads the game into values and
// hands them to core::hands::Haptics.

#include "loader/hooks/haptic_hooks.h"

#include "core/diag/thread_roles.h"
#include "core/diag/tuning.h"
#include "core/input/interaction_state.h"
#include "game/constants.h"
#include "game/hands/hand_objects.h"
#include "game/hands/haptic_sites.h"
#include "game/patcher.h"
#include "loader/context.h"
#include "loader/hooks/guard.h"
#include "loader/hooks/hand_hooks.h"
#include "util/sync/frame_period.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

namespace hands = core::hands;
using core::diag::ports::logf;
using game::hands::HandTargets;

constexpr std::string_view kLogSettings =
    "haptics: on (Strength {:.2f}, GameRumble {}, Damage {}, Tracker {}, Firing {}, Hacking {})";
constexpr std::string_view kLogUnverified =
    "haptics: no pulse for hacking steps or the Alien close by: no verified game signal (21 §22 items 18, "
    "19)";
constexpr std::string_view kLogTrackerGate =
    "haptics: the tracker ticks while the VR tracker control holds it raised (whether the game scans with "
    "it lowered is open, 21 §22 item 7)";
constexpr std::string_view kLogNoSource = "haptics: off; none of their game sources is in place";
constexpr std::string_view kLogFirstRumble =
    "haptics: the game set its rumble for the first time (thread {} {}, low {:.2f} high {:.2f})";
constexpr std::string_view kLogRumbleStats =
    "[diag] haptics: game rumble set {} times since the last line, last low {:.2f} high {:.2f}";

// The player map's tier of the virtual pad (04 §3): the VR fire and tracker
// controls as the game is given them.
constexpr std::size_t kPlayerTier = 0;

// Which sources went in, set at attach before the game's threads run.
game::hands::HapticSitesOutcome sources{};
bool installed = false;

// The rumble hook's thread, the game thread's debug line: how often the
// game sets its rumble (21 §22 item 9). Relaxed: counters for a log line.
std::atomic<std::uint32_t> rumble_calls{0};
std::atomic<float> last_low{0.0f};
std::atomic<float> last_high{0.0f};
std::atomic<bool> rumble_seen{false};
util::FramePeriod stats_period{core::diag::kDebugLogPeriodFrames}; // game thread

// The game's RumbleSetMotors (21 §2.9): records the levels, then sets the
// pad as before (21 §14.3: a physical pad still rumbles). __thiscall, two
// floats, ret 8: __fastcall with the unused EDX.
int __fastcall rumble_set_motors(void* self, void* edx, float low, float high) noexcept {
    auto& ctx = context();
    guarded("RumbleSetMotors", [&] {
        ctx.haptics.on_rumble(low, high, ctx.clock.now());
        rumble_calls.fetch_add(1, std::memory_order_relaxed);
        last_low.store(low, std::memory_order_relaxed);
        last_high.store(high, std::memory_order_relaxed);
        if (rumble_seen.exchange(true, std::memory_order_relaxed)) return;
        const std::uint32_t id = ctx.current_thread.id();
        logf(ctx.log, kLogFirstRumble, id, core::diag::thread_name(ctx.thread_roles.role_of(id)), low, high);
    });
    return ctx.targets.haptics.rumble_set_motors(self, edx, low, high);
}

static_assert(std::is_convertible_v<decltype(&rumble_set_motors), game::hands::RumbleSetMotorsFn>);

bool vr_down(Context& ctx, int action) noexcept {
    return ctx.pad.tiers()[kPlayerTier].digital[static_cast<std::size_t>(action)] != 0;
}

hands::FireSample read_fire(Context& ctx, void* player) noexcept {
    hands::FireSample f;
    f.pressed = vr_down(ctx, game::player_action::kFire);
    // CurrentItem only while the control is held: a press is all that pulses.
    if (!f.pressed || !player) return f;
    void* component = game::hands::weapon_component(player);
    if (!component) return f;
    f.weapon    = game::hands::current_weapon_type(ctx.targets.hands, component);
    f.gate_open = game::hands::fire_aligned(component);
    return f;
}

void log_rumble_stats(Context& ctx) noexcept {
    if (!stats_period.tick() || !ctx.log.debug_enabled()) return;
    logf(ctx.log, kLogRumbleStats, rumble_calls.exchange(0, std::memory_order_relaxed),
         last_low.load(std::memory_order_relaxed), last_high.load(std::memory_order_relaxed));
}

void log_settings(Context& ctx) noexcept {
    const auto& s = ctx.settings.current().hands.haptics;
    logf(ctx.log, kLogSettings, s.strength, s.game_rumble, s.damage, s.tracker, s.firing, s.hacking);
    ctx.log.line(kLogUnverified);
}

} // namespace

void prepare_haptics(const game::GameImage& g) noexcept {
    auto& ctx = context();
    if (!ctx.settings.current().hands.haptics.enabled) return;
    log_settings(ctx);
    game::Patcher patcher{ctx.log};
    const game::hands::HapticHooks hooks{&rumble_set_motors, tracker_scan_hook()};
    sources   = game::hands::install_haptic_sites(g, patcher, hooks, ctx.targets.hands, ctx.targets.haptics);
    installed = sources.rumble || sources.player || sources.tracker;
    if (!installed) ctx.log.line(kLogNoSource);
    if (sources.tracker) ctx.log.line(kLogTrackerGate);
}

void haptics_on_head_pose() noexcept {
    if (!installed || !sources.player) return;
    auto& ctx = context();
    guarded("haptics", [&] {
        void* player = game::hands::player_character(ctx.targets.hands);
        ctx.haptics.on_health(player ? std::optional{game::hands::health(player)} : std::nullopt);
        if (sources.firing) ctx.haptics.on_fire(read_fire(ctx, player), ctx.clock.now());
        if (sources.rumble) log_rumble_stats(ctx);
    });
}

void haptics_on_interaction() noexcept {
    if (!installed) return;
    auto& ctx       = context();
    const auto kind = ctx.interactions.interaction();
    ctx.haptics.on_interaction(
        ctx.interactions.in_interaction() &&
        (kind == core::input::Interaction::Hacking || kind == core::input::Interaction::Cutting));
}

void haptics_on_tracker_scan() noexcept {
    if (!installed || !sources.tracker) return;
    auto& ctx = context();
    ctx.haptics.on_tracker_scan(vr_down(ctx, game::player_action::kTracker));
}

bool haptics_installed() noexcept { return installed; }

} // namespace omvr::loader::hooks
