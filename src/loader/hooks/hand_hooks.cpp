// OpenMotherVR-Provenance: clean
// The tracked hands' hooks (spec 21 §3 to §6). Each reads the game into
// values, asks core::hands::Hands and writes the result back.

#include "loader/hooks/hand_hooks.h"

#include "core/hands/hand_aim.h"
#include "core/hands/types.h"
#include "core/stereo/types.h"
#include "game/hands/hand_objects.h"
#include "game/hands/hand_sites.h"
#include "game/patcher.h"
#include "game/stereo/stereo_objects.h"
#include "loader/context.h"
#include "loader/hooks/activity_input.h"
#include "loader/hooks/guard.h"
#include "loader/hooks/haptic_hooks.h"

#include <atomic>
#include <optional>
#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

namespace hands = core::hands;
using game::hands::HandTargets;

constexpr std::string_view kLogArmsOff =
    "hands: the arms keep the game's animation in this build: the grip to hand-bone mapping, held items "
    "and the model-to-world transform are not known yet (21 §22 items 2, 3); aiming and the tracker follow "
    "the hands";

// 21 §4.2 item 6: the held item is drawn in the hand the arm edit moved.
// This build has no arm pose edit (the arms keep the game's animation), so
// the weapon is where the game's animation put it and the aim keeps the
// right hand's aim pose (21 §5.2); the drawn left hand is not the
// controller either, so the tracker faces by the left hand's aim pose
// (21 §6.2's fallback).
constexpr bool kArmsFollowHands = false;

// The thresholds of the fire alignment test (21 §5.3). The game's code reads
// `fire_gate_threshold` at a fixed address once its test is pointed here:
// a global, as the patched code needs one. It holds the game's own value
// (`game_fire_gate_threshold`, read at install) except while the hand aims.
// Written on the game thread; an aligned float store is atomic on x86.
alignas(sizeof(float)) float fire_gate_threshold = 0.0f;
float game_fire_gate_threshold                   = 0.0f;

// The game image as the installer saw it, for an install once Hands.Enabled
// is turned on in play; and whether the sites were tried. Game thread
// (attach runs before the game's threads).
std::optional<game::GameImage> image;
bool sites_tried = false;

using AimRayUpdateFn = void(__fastcall*)(void* self, void* edx);
using TrackerScanFn  = bool(__fastcall*)(void* self, void* edx);
static_assert(std::is_convertible_v<decltype(&aim_ray_update), AimRayUpdateFn>);
static_assert(std::is_convertible_v<decltype(&tracker_scan), TrackerScanFn>);

void install_sites(Context& ctx) noexcept {
    sites_tried = true;
    if (!image) return;
    game::Patcher patcher{ctx.log};
    const game::hands::HandHooks hooks{game::code_pointer(&aim_ray_update), game::code_pointer(&tracker_scan),
                                       &fire_gate_threshold};
    game::hands::install_hand_sites(*image, patcher, hooks, ctx.targets.hands);
    game_fire_gate_threshold = fire_gate_threshold;
    ctx.log.line(kLogArmsOff);
}

hands::HandFrame read_frame(core::vr::ports::VrRuntime& runtime, const util::Matrix34& head, float s) {
    hands::HandFrame f;
    f.head = head;
    for (std::size_t h = 0; h < hands::kHandCount; ++h) {
        auto& sample          = f.hands[h];
        const int hand        = static_cast<int>(h);
        sample.grip_valid     = runtime.hand_pose(hand, sample.grip, s);
        sample.aim_valid      = runtime.aim_pose(hand, sample.aim, s);
        float v[3]            = {};
        sample.velocity_valid = runtime.hand_velocity(hand, v, s);
        sample.velocity       = {v[0], v[1], v[2]};
    }
    return f;
}

// 21 §4.4: the frame's signals, all from v1's own state.
hands::ActivityInput read_activity(Context& ctx, game::StereoInterface& si) noexcept {
    return activity_input(ctx, si, game::hands::player_character(ctx.targets.hands));
}

} // namespace

void prepare_hands(const game::GameImage& g) noexcept {
    image.emplace(g);
    auto& ctx = context();
    if (ctx.settings.current().hands.enabled) install_sites(ctx);
}

void hands_on_head_pose(game::StereoInterface& si, core::vr::ports::VrRuntime& runtime,
                        const util::Matrix34& head) noexcept {
    auto& ctx = context();
    guarded("hands", [&] {
        const auto now = ctx.clock.now();
        if (!ctx.settings.current().hands.enabled) {
            ctx.hands.update({}, {}, now, si.world_scale);
            return;
        }
        if (!sites_tried) install_sites(ctx);
        ctx.hands.update(read_frame(runtime, head, si.world_scale), read_activity(ctx, si), now,
                         si.world_scale);
    });
}

void __fastcall aim_ray_update(void* component, void*) noexcept {
    auto& ctx            = context();
    const HandTargets& t = ctx.targets.hands;
    game::hands::call_aim_ray_update(t, component);
    guarded("AimRayUpdate", [&] {
        const auto now = ctx.clock.now();
        const auto cam = ctx.hands.aim_wanted(now) ? game::hands::active_camera(t) : std::nullopt;
        if (!cam) {
            fire_gate_threshold = game_fire_gate_threshold;
            return;
        }
        const auto weapon = game::hands::current_weapon(t, component);
        hands::WeaponSample sample{weapon.type, std::nullopt, kArmsFollowHands};
        if (ctx.hands.weapon_transform_wanted(sample.type, sample.drawn_in_moved_hand, now)) {
            sample.transform = game::hands::weapon_transform(t, weapon.object);
        }
        const auto r        = ctx.hands.aim(game::hands::read_aim_ray(component), *cam, sample, now);
        fire_gate_threshold = hands::fire_gate_threshold(r.hand_aiming, game_fire_gate_threshold);
        if (r.ray) game::hands::write_aim_ray(component, *r.ray);
    });
}

bool __fastcall tracker_scan(void* tracker, void*) noexcept {
    auto& ctx            = context();
    const HandTargets& t = ctx.targets.hands;
    // 21 §6.2: the facing is the hand's for the scan only; the game's value
    // is put back after it, so nothing else that reads it sees the hand.
    std::optional<game::hands::TrackerFacing> saved;
    guarded("TrackerScan", [&] {
        const auto cam = game::hands::active_camera(t);
        if (!cam) return;
        const auto facing = ctx.hands.tracker_facing(*cam, std::nullopt, ctx.clock.now());
        if (!facing) return;
        saved = game::hands::read_tracker_facing(tracker);
        game::hands::write_tracker_facing(tracker, {facing->x, facing->y});
    });
    const bool result = game::hands::call_tracker_scan(t, tracker);
    if (saved) game::hands::write_tracker_facing(tracker, *saved);
    guarded("TrackerScan haptics", [&] { haptics_on_tracker_scan(tracker, result); });
    return result;
}

const void* tracker_scan_hook() noexcept { return game::code_pointer(&tracker_scan); }

} // namespace omvr::loader::hooks
