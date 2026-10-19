// OpenMotherVR-Provenance: clean
// The tracked hands' hooks (spec 21 §3 to §6). Each reads the game into
// values, asks core::hands::Hands and writes the result back.

#include "loader/hooks/hand_hooks.h"

#include "core/input/action_tiers.h"
#include "core/hands/action_episode.h"
#include "core/hands/hand_aim.h"
#include "core/hands/motion_actions.h"
#include "core/hands/types.h"
#include "core/stereo/types.h"
#include "game/code_address.h"
#include "game/constants.h"
#include "game/hands/action_sites.h"
#include "game/hands/arm_objects.h"
#include "game/hands/arm_sites.h"
#include "game/hands/body_objects.h"
#include "game/hands/hand_objects.h"
#include "game/hands/hand_sites.h"
#include "game/patcher.h"
#include "game/stereo/stereo_objects.h"
#include "loader/context.h"
#include "loader/hooks/activity_input.h"
#include "loader/hooks/body_hooks.h"
#include "loader/hooks/guard.h"
#include "loader/hooks/haptic_hooks.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <intrin.h>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace omvr::loader::hooks {

namespace {

namespace hands = core::hands;
using game::hands::HandTargets;

constexpr std::string_view kLogArmsNoSkin =
    "[hook] hands, arms: off; FinalPoseStep's SkinSetPose calls are not in place, so the arms keep the "
    "game's animation";

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
// The arm pose edit's sites (21 §4.2): tried at the first head pose with
// Hands.Enabled on (after attach has prepared body visibility's), and
// whether the held item's attachment query went in. Game thread; the
// targets are set before the first redirect and only read after it.
bool arms_tried      = false;
bool item_follows_in = false;
game::hands::ArmTargets arm_targets;
// 21 §4.2 item 6: while the game thread runs the player's equipment update
// again after the pass, the animation component whose held item is placed
// from the final pose (0 otherwise).
std::atomic<std::uintptr_t> attach_override{0};

constexpr std::string_view kLogPlayAnimation =
    "[diag] hands: play-animation {} (command {:08X}, arguments {:08X} {:08X}); the scripted animation "
    "episode is not used until a request's character is known (21 §22 item 15)";
constexpr std::string_view kLogLaunch =
    "[diag] hands: throw by motion: a velocity set {:.3f} s after the release (body {:08X}, caller RVA "
    "{:08X}): linear ({:+.2f} {:+.2f} {:+.2f}); the hand's ({:+.2f} {:+.2f} {:+.2f})";

// The player map's tier of the virtual pad (04 §3): the controls as the
// game is given them.
constexpr std::size_t kPlayerTier = 0;

// The trampolines of the player action sites, set before each detour goes
// live (attach, or the game thread when Hands.Enabled is turned on in play).
game::hands::ActionSiteFns action_targets;

// The throw launch log (21 §22 item 8): which throw the lines belong to
// (its stamp) and how many were written for it. Relaxed: counters for a
// log line, on whichever thread sets the velocities.
std::atomic<std::int64_t> launch_throw{0};
std::atomic<std::uint32_t> launch_lines{0};

using AimRayUpdateFn = void(__fastcall*)(void* self, void* edx);
using TrackerScanFn  = bool(__fastcall*)(void* self, void* edx);
static_assert(std::is_convertible_v<decltype(&aim_ray_update), AimRayUpdateFn>);
static_assert(std::is_convertible_v<decltype(&tracker_scan), TrackerScanFn>);

// ---- Player actions (21 §2.11, §8.2, §12.1) ----------------------------------------

// 21 §8.2 (r11): a start of the player's melee action (either method)
// hands the right arm to the game's swing.
template <game::hands::ActionStartFn game::hands::ActionSiteFns::* Original>
std::uintptr_t __fastcall melee_start(void* self, void* edx) noexcept {
    auto& ctx = context();
    guarded("melee start", [&] { ctx.hands.on_melee_start(ctx.clock.now()); });
    return (action_targets.*Original)(self, edx);
}

// 21 §12.1 (r11): a start of a scripted player action begins an episode.
template <hands::PlayerAction Action, game::hands::ActionStartFn game::hands::ActionSiteFns::* Original>
std::uintptr_t __fastcall action_start(void* self, void* edx) noexcept {
    auto& ctx = context();
    guarded("player action start", [&] { ctx.hands.on_player_action(Action, ctx.clock.now()); });
    return (action_targets.*Original)(self, edx);
}

// 21 §2.11, §18: the play request and its counterpart, logged with Dev.
// Debug Options only (installed only then).
template <game::hands::PlayAnimationFn game::hands::ActionSiteFns::* Original>
std::uintptr_t __fastcall play_animation(void* self, void* edx, void* first, void* second) noexcept {
    auto& ctx = context();
    guarded("play animation", [&] {
        const bool request = Original == &game::hands::ActionSiteFns::play_request;
        core::diag::ports::logf(ctx.log, kLogPlayAnimation, request ? "request" : "counterpart",
                                game::address_of(self), game::address_of(first), game::address_of(second));
    });
    return (action_targets.*Original)(self, edx, first, second);
}

// 21 §22 item 8: the first velocity sets after a throw by motion, with the
// hand's launch velocity next to each, to find which one is the throw.
void log_launch_candidate(Context& ctx, const void* body, const float* linear, const void* caller) noexcept {
    if (!linear || !ctx.log.debug_enabled() || !image) return;
    const auto now = ctx.clock.now();
    const auto r   = ctx.hands.recent_throw(now);
    if (!r) return;
    const std::int64_t stamp = util::stamp_ticks(r->at);
    if (launch_throw.exchange(stamp, std::memory_order_relaxed) != stamp)
        launch_lines.store(0, std::memory_order_relaxed);
    if (launch_lines.fetch_add(1, std::memory_order_relaxed) >= hands::kThrowLaunchLines) return;
    const auto cam = game::hands::active_camera(ctx.targets.hands);
    const util::Vec3 hand =
        cam ? hands::throw_velocity(*cam, r->local, ctx.settings.current().hands.throw_strength)
            : util::Vec3{};
    core::diag::ports::logf(ctx.log, kLogLaunch, util::seconds_between(r->at, now).count(),
                            game::address_of(body), image->rva_of(static_cast<const std::uint8_t*>(caller)),
                            linear[0], linear[1], linear[2], hand.x, hand.y, hand.z);
}

// SetBodyVelocities (21 §2.7), installed with Dev. Debug Options and
// Hands.ThrowByMotion: logs, then sets the velocities as the game asked.
std::uintptr_t __cdecl set_body_velocities(void* body, const float* linear, const float* angular) noexcept {
    auto& ctx          = context();
    const void* caller = _ReturnAddress();
    guarded("SetBodyVelocities", [&] { log_launch_candidate(ctx, body, linear, caller); });
    return action_targets.set_body_velocities(body, linear, angular);
}

using game::hands::ActionSiteFns;
static_assert(
    std::is_convertible_v<decltype(&melee_start<&ActionSiteFns::melee_a>), game::hands::ActionStartFn>);
static_assert(std::is_convertible_v<decltype(&play_animation<&ActionSiteFns::play_request>),
                                    game::hands::PlayAnimationFn>);
static_assert(std::is_convertible_v<decltype(&set_body_velocities), game::hands::SetBodyVelocitiesFn>);

void install_action_sites(Context& ctx, game::Patcher& patcher) noexcept {
    using hands::PlayerAction;
    ActionSiteFns hooks;
    hooks.melee_a             = &melee_start<&ActionSiteFns::melee_a>;
    hooks.melee_b             = &melee_start<&ActionSiteFns::melee_b>;
    hooks.melee_kill          = &action_start<PlayerAction::MeleeKill, &ActionSiteFns::melee_kill>;
    hooks.trap_break          = &action_start<PlayerAction::TrapBreak, &ActionSiteFns::trap_break>;
    hooks.melee_intercept     = &action_start<PlayerAction::MeleeIntercept, &ActionSiteFns::melee_intercept>;
    hooks.impact_standing     = &action_start<PlayerAction::ImpactStanding, &ActionSiteFns::impact_standing>;
    hooks.trap_victim         = &action_start<PlayerAction::TrapVictim, &ActionSiteFns::trap_victim>;
    hooks.play_request        = &play_animation<&ActionSiteFns::play_request>;
    hooks.play_counterpart    = &play_animation<&ActionSiteFns::play_counterpart>;
    hooks.set_body_velocities = &set_body_velocities;
    const bool debug          = ctx.log.debug_enabled();
    const game::hands::ActionSitesWanted wanted{true, debug,
                                                debug && ctx.settings.current().hands.throw_by_motion};
    game::hands::install_action_sites(*image, patcher, hooks, wanted, action_targets);
}

// ---- The arm pose edit (21 §4.2) ---------------------------------------------------

std::uintptr_t id_of(const void* p) noexcept { return game::address_of(p); }

// The player's pose as the arm edit names it.
std::optional<std::pair<game::hands::PlayerPose, hands::PlayerSkeleton>> player_skeleton(
    void* player) noexcept {
    const auto pose = game::hands::player_pose(player);
    if (!pose) return std::nullopt;
    return std::pair{*pose,
                     hands::PlayerSkeleton{id_of(pose->animation), id_of(pose->pose), id_of(pose->skeleton),
                                           game::hands::skeleton_name(pose->skeleton)}};
}

// Game thread, just before the frame's final-pose pass: this frame's arm
// targets for the jobs (21 §17). Returns the player's animation component
// when they were published.
void* before_final_pose(Context& ctx, std::optional<hands::CameraPose>& camera) noexcept {
    hands::ArmPoser& arms = ctx.hands.arms();
    const auto now        = ctx.clock.now();
    void* player =
        ctx.settings.current().hands.enabled ? game::hands::player_character(ctx.targets.hands) : nullptr;
    const auto found = player ? player_skeleton(player) : std::nullopt;
    camera           = found ? game::hands::active_camera(ctx.targets.hands) : std::nullopt;
    if (!found || !camera) {
        arms.publish_none();
        return nullptr;
    }
    const auto& [pose, skeleton] = *found;
    if (arms.wants_rig(skeleton, now)) {
        game::hands::SkeletonRead read;
        const SkeletonBonesRead bones = read_skeleton_bones(skeleton.skeleton, read);
        arms.set_rig(skeleton, bones.bones ? &*bones.bones : nullptr, bones.fault, now);
    }
    arms.publish(skeleton, *camera, game::hands::model_to_world(pose.animation),
                 ctx.hands.arm_targets(*camera, now), now);
    return pose.animation;
}

// Game thread, after the pass: the held item placed on the edited right
// hand (21 §4.2 item 6: the equipment update again, its attachment query
// reading the final pose), then the report's log lines.
void after_final_pose(Context& ctx, void* animation, const hands::CameraPose& camera) noexcept {
    hands::ArmPoser& arms = ctx.hands.arms();
    bool follows          = false;
    if (item_follows_in && arms.right_edited()) {
        void* player    = game::hands::player_character(ctx.targets.hands);
        void* equipment = game::hands::equipment_component(player);
        if (equipment) {
            attach_override.store(id_of(animation), std::memory_order_release);
            game::hands::call_equipment_update(arm_targets, equipment);
            attach_override.store(0, std::memory_order_release);
            follows = true;
        }
    }
    std::optional<util::Vec3> head;
    if (const auto bone = arms.head_check_due())
        head =
            game::hands::bone_world_position(arm_targets, animation, *bone, game::hands::kFinalPoseSelector);
    arms.after_pass(head, camera, follows, ctx.clock.now());
}

// The stage's call of the final-pose pass (21 §2.1): the arm targets are
// published before it and the held item follows after it, all on the game
// thread; the pass itself runs the jobs and waits for them.
std::uintptr_t __cdecl final_pose_pass(void* list, std::uint32_t count) noexcept {
    auto& ctx       = context();
    void* animation = nullptr;
    std::optional<hands::CameraPose> camera;
    guarded("arm targets", [&] { animation = before_final_pose(ctx, camera); });
    const std::uintptr_t r = game::hands::call_final_pose_pass(arm_targets, list, count);
    if (animation && camera) guarded("arm report", [&] { after_final_pose(ctx, animation, *camera); });
    return r;
}

// The held item's attachment query (21 §2.3, §4.2 item 6): selector 6 as
// the game asks, except for the player's item while it is placed again
// from the final pose (selector 7).
void* __fastcall attach_query(void* animation, void*, int bone, int selector, void* out) noexcept {
    const std::uintptr_t over = attach_override.load(std::memory_order_acquire);
    if (over != 0 && id_of(animation) == over && selector == game::hands::kAttachPoseSelector)
        selector = game::hands::kFinalPoseSelector;
    return game::hands::call_bone_world_transform(arm_targets, animation, bone, selector, out);
}

// The game's limb IK (21 §4.2 item 7): not run on the player's
// first-person pose while its arms are edited; as the game asks otherwise.
std::uintptr_t __stdcall limb_ik(void* setup, void* second, void* pose, void* fourth) noexcept {
    if (pose && context().hands.arms().limb_ik_off(id_of(game::hands::pose_skeleton(pose)))) return 0;
    return game::hands::call_limb_ik(arm_targets, setup, second, pose, fourth);
}

static_assert(std::is_convertible_v<decltype(&final_pose_pass), game::hands::FinalPosePassFn>);
static_assert(std::is_convertible_v<decltype(&attach_query), game::hands::BoneWorldTransformFn>);
static_assert(std::is_convertible_v<decltype(&limb_ik), game::hands::LimbIkFn>);

// 21 §4.2: the edit runs in the SkinSetPose calls (body visibility's sites)
// and is placed around the final-pose pass. Needs the player.
void install_arms(Context& ctx) noexcept {
    arms_tried = true;
    if (!image || !ctx.targets.hands.registry) return;
    if (!body_pose_sites()) {
        ctx.log.line(kLogArmsNoSkin);
        return;
    }
    game::Patcher patcher{ctx.log};
    const game::hands::ArmHooks hooks{game::code_pointer(&final_pose_pass), game::code_pointer(&attach_query),
                                      game::code_pointer(&limb_ik)};
    item_follows_in = game::hands::install_arm_sites(*image, patcher, hooks, arm_targets).held_item;
}

void install_sites(Context& ctx) noexcept {
    sites_tried = true;
    if (!image) return;
    game::Patcher patcher{ctx.log};
    const game::hands::HandHooks hooks{game::code_pointer(&aim_ray_update), game::code_pointer(&tracker_scan),
                                       &fire_gate_threshold};
    const auto o             = game::hands::install_hand_sites(*image, patcher, hooks, ctx.targets.hands);
    game_fire_gate_threshold = fire_gate_threshold;
    // The player actions need the player (the hand sites found it).
    if (o.aim || o.tracker) install_action_sites(ctx, patcher);
}

// 21 §8.2, §9.2: the current item and the Aim control for melee and
// throwing by motion; the item only when either is on.
hands::PlayInput read_play(Context& ctx, void* player) noexcept {
    hands::PlayInput p;
    p.aim_held     = core::input::vr_digital(ctx.pad.tiers()[kPlayerTier],
                                             static_cast<std::size_t>(game::player_action::kAim));
    const auto& hs = ctx.settings.current().hands;
    if (!player || !ctx.targets.hands.current_item || !(hs.melee_by_motion || hs.throw_by_motion)) return p;
    void* component = game::hands::weapon_component(player);
    if (!component) return p;
    p.item_known = true;
    p.weapon     = game::hands::current_weapon_type(ctx.targets.hands, component);
    return p;
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
            ctx.hands.update({}, {}, {}, now, si.world_scale);
            return;
        }
        if (!sites_tried) install_sites(ctx);
        if (!arms_tried) install_arms(ctx);
        // 21 §4.4: the frame's signals, all from v1's own state.
        void* player = game::hands::player_character(ctx.targets.hands);
        const auto out =
            ctx.hands.update(read_frame(runtime, head, si.world_scale), activity_input(ctx, si, player),
                             read_play(ctx, player), now, si.world_scale);
        if (out.press_fire_until) ctx.body_controls.press_fire_until(*out.press_fire_until);
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
        // 21 §5.2 (r11): the weapon's own transform aims while it is drawn
        // in the hand the arm edit moved (21 §4.2 item 6).
        hands::WeaponSample sample{weapon.type, std::nullopt, ctx.hands.arms().weapon_in_moved_hand(now)};
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
        // 21 §6.2 (r11): the drawn left hand from the pose the player saw
        // last, when the arm edit drew it.
        const auto now    = ctx.clock.now();
        const auto facing = ctx.hands.tracker_facing(*cam, ctx.hands.arms().drawn_left(now), now);
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

void hands_on_skin_pose(const void* model) noexcept {
    hands::ArmPoser& arms    = context().hands.arms();
    const hands::ArmFrame* f = arms.pending();
    if (!f) return;
    void* pose = game::CodeAddress{static_cast<std::uint32_t>(f->pose)};
    if (game::hands::pose_model_data(pose) != model || !arms.claim(*f)) return;
    // 21 §4.2, §17: on the player's final-pose job, from the game thread's
    // copy only; the body's model-to-world transform as it is now.
    const void* animation = game::CodeAddress{static_cast<std::uint32_t>(f->animation)};
    auto arrays           = game::hands::pose_arrays(pose).value_or(hands::PoseArrays{});
    arms.apply(*f, game::hands::model_to_world(animation), arrays);
}

} // namespace omvr::loader::hooks
