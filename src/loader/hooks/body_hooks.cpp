// OpenMotherVR-Provenance: clean
// The body features' hooks (spec 21 §10 to §12). Each reads the game into
// values, asks core::hands' body objects and writes the result back.

#include "loader/hooks/body_hooks.h"

#include "core/config/settings_table.h"
#include "core/hands/activity.h"
#include "core/hands/tuning.h"
#include "core/hands/body_visibility.h"
#include "game/code_address.h"
#include "game/hands/body_objects.h"
#include "game/hands/body_sites.h"
#include "game/hands/hand_objects.h"
#include "game/hands/hand_sites.h"
#include "game/patcher.h"
#include "game/stereo/stereo_objects.h"
#include "loader/context.h"
#include "loader/hooks/activity_input.h"
#include "loader/hooks/guard.h"
#include "util/sync/once_latch.h"
#include "util/time/steady.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

namespace hands = core::hands;
using core::config::BodyVisible;
using core::diag::ports::logf;

constexpr std::string_view kLogRoomScaleOff =
    "[body] room-scale: off in this build; the frame the game's movement input is taken in and the "
    "body's yaw in the tracking space are not established (21 §22 items 25, 26)";
constexpr std::string_view kLogCrouchPress =
    "[body] real crouch: {} (head {:.2f} m, crouch below {:.2f} m, stand above {:.2f} m)";
constexpr std::string_view kLogCrouchNotFollowed =
    "[body] WARNING real crouch: the game's requested crouch did not follow a press within {:.1f} s "
    "(21 §22 item 28)";
constexpr std::string_view kLogHandBack = "[body] control hand-back after {}: {}";

static_assert(game::hands::kMaxSkeletonBones == hands::kMaxBodyBones, "the reader reads what the plan holds");
static_assert(std::is_convertible_v<decltype(&skin_set_pose), game::hands::SkinSetPoseFn>);

// The game image as the installer saw it, for an install once Body.Visible
// leaves Full in play; whether the sites were tried and went in; the
// player lookup (21 §2.2), found the first time a body feature needs it.
// Game thread, except `targets` and `sites_in`, set before the redirect
// and read by the job threads after it.
std::optional<game::GameImage> image;
bool sites_tried  = false;
bool sites_in     = false;
bool player_tried = false;
game::hands::BodyTargets targets;
game::hands::HandTargets player_lookup;

// The pose the player's render objects receive while parts are hidden:
// written only by the player's final-pose job, which calls SkinSetPose once
// per object in turn (21 §2.3, §17). A loader global, as the hook has
// nowhere else to keep it without allocating.
alignas(16) std::array<hands::BoneTransform, hands::kMaxBodyBones> hidden_pose;

util::OnceLatch room_scale_logged;
util::OnceLatch crouch_not_followed_logged;

std::uintptr_t id_of(const void* p) noexcept { return game::address_of(p); }

void install_sites(Context& ctx) noexcept {
    sites_tried = true;
    if (!image) return;
    game::Patcher patcher{ctx.log};
    sites_in = game::hands::install_body_sites(*image, patcher, game::code_pointer(&skin_set_pose), targets);
}

void* find_player_character(Context& ctx) noexcept {
    if (!player_tried) {
        player_tried = true;
        if (image) game::hands::find_player(*image, ctx.log, player_lookup);
    }
    return game::hands::player_character(player_lookup);
}

// 21 §12.1: the episodes, from the signals the hands use (21 §4.4).
hands::Activity read_activity(Context& ctx, game::StereoInterface& device, void* player) noexcept {
    return hands::activity(activity_input(ctx, device, player));
}

hands::SkeletonFault fault_of(game::hands::SkeletonReadResult r) noexcept {
    using R = game::hands::SkeletonReadResult;
    switch (r) {
        case R::Ok: break;
        case R::NoBones: return hands::SkeletonFault::NoBones;
        case R::NoArrays: return hands::SkeletonFault::NoArrays;
        case R::TooManyBones: return hands::SkeletonFault::TooManyBones;
        case R::UnnamedBone: return hands::SkeletonFault::UnnamedBone;
        case R::TableMismatch: return hands::SkeletonFault::TableMismatch;
    }
    return hands::SkeletonFault::Unknown;
}

// 21 §10: the plan for this frame's skeleton, published for the jobs. A
// skeleton that cannot be read yet is read again until it can be (21 §10.2).
void update_visibility(Context& ctx, BodyVisible v, bool running, void* player,
                       util::TimePoint now) noexcept {
    if (v != BodyVisible::Full && !sites_tried) install_sites(ctx);
    if (!sites_in) v = BodyVisible::Full;
    hands::PlayerSkeleton skeleton;
    if (const auto pose = v != BodyVisible::Full ? game::hands::player_pose(player) : std::nullopt) {
        skeleton = {id_of(pose->animation), id_of(pose->pose), id_of(pose->skeleton),
                    game::hands::skeleton_name(pose->skeleton)};
    }
    std::optional<hands::SkeletonBones> bones;
    hands::SkeletonReadFault fault;
    // Read only when asked: once per change, and once a second while the
    // skeleton cannot be read (no walk of the bones every frame).
    game::hands::SkeletonRead read;
    if (ctx.body_hider.wants_bones(v, skeleton, now)) {
        const auto info = game::hands::read_skeleton(
            game::CodeAddress{static_cast<std::uint32_t>(skeleton.skeleton)}, read);
        if (info.result == game::hands::SkeletonReadResult::Ok) {
            bones = hands::SkeletonBones{std::span{read.names}.first(read.count),
                                         std::span{read.parents}.first(read.count), read.names_from_table};
        } else {
            fault = {fault_of(info.result), info.raw_count, info.bone};
        }
    }
    ctx.body_hider.update(v, running, skeleton, bones ? &*bones : nullptr, now, fault);
}

std::string_view press_name(hands::CrouchPress p) noexcept {
    return p == hands::CrouchPress::Crouch ? "crouch" : "stand";
}

// 21 §11.5, §12.2 item 3.
void update_crouch(Context& ctx, game::StereoInterface& device, void* player, hands::Activity activity,
                   util::TimePoint now) noexcept {
    const auto& s = ctx.settings.current();
    hands::RealCrouchInput in;
    in.enabled               = s.body.real_crouch;
    in.standing              = ctx.play_space.applied_mode() == core::config::PlayMode::Standing;
    in.standing_eye_height_m = s.comfort.standing_eye_height;
    if (auto* runtime = game::load_backend(device);
        in.enabled && runtime && runtime->has_floor() && runtime->headset_in_use()) {
        in.head_height_m = runtime->head_height_above_floor();
    }
    in.controllable  = activity == hands::Activity::Active;
    in.game_crouched = game::hands::requested_crouch(player);

    const hands::RealCrouchOutput out = ctx.real_crouch.update(in, now);
    if (out.press != hands::CrouchPress::None) {
        ctx.body_controls.press_crouch_until(util::after(now, hands::kCrouchPress));
        if (ctx.log.debug_enabled())
            logf(ctx.log, kLogCrouchPress, press_name(out.press), in.head_height_m.value_or(0.0f),
                 out.crouch_below_m, out.stand_above_m);
    }
    if (out.press_not_followed && crouch_not_followed_logged.first())
        logf(ctx.log, kLogCrouchNotFollowed, hands::kCrouchConfirm.count());
    ctx.play_space.set_crouch_raise(out.eye_raise_m);
}

} // namespace

void prepare_body(const game::GameImage& g) noexcept {
    image.emplace(g);
    auto& ctx = context();
    if (ctx.settings.current().body.visible != BodyVisible::Full) install_sites(ctx);
}

void body_on_stereo_update(game::StereoInterface& device) noexcept {
    auto& ctx = context();
    guarded("body", [&] {
        const auto& body = ctx.settings.current().body;
        if (body.room_scale && room_scale_logged.first()) ctx.log.line(kLogRoomScaleOff);
        const bool needs_player = body.visible != BodyVisible::Full || body.real_crouch;
        void* player            = needs_player ? find_player_character(ctx) : nullptr;
        const bool running      = game::stereo::load_state(device) == game::DeviceState::Running;
        const auto now          = ctx.clock.now();

        update_visibility(ctx, body.visible, running, player, now);
        const hands::Activity activity = read_activity(ctx, device, player);
        if (const auto ended = ctx.hand_back.update(activity)) {
            ctx.real_crouch.on_hand_back(now);
            if (ctx.log.debug_enabled())
                logf(ctx.log, kLogHandBack, hands::to_string(*ended),
                     ctx.real_crouch.active() ? "crouch re-synced after 0.5 s" : "nothing to restore");
        }
        update_crouch(ctx, device, player, activity, now);
    });
}

void __fastcall skin_set_pose(void* object, void* edx, const void* pose, int count) noexcept {
    auto& ctx                       = context();
    const hands::HiddenPose* hidden = ctx.body_hider.current();
    game::StereoInterface* device   = game::stereo::load_device(ctx.manager);
    if (hidden && device && game::stereo::load_state(*device) == game::DeviceState::Running) {
        const void* player_pose = game::CodeAddress{static_cast<std::uint32_t>(hidden->pose)};
        const auto n            = static_cast<std::size_t>(count);
        const bool players      = pose == game::hands::pose_model_data(player_pose) &&
                                  id_of(game::hands::pose_skeleton(player_pose)) == hidden->skeleton;
        if (players && count > 0 && n == hidden->plan.count) {
            // 21 §10.2: the render objects receive a copy with the hidden
            // bones collapsed; the game's own pose is not changed.
            const std::span src{static_cast<const hands::BoneTransform*>(pose), n};
            hands::apply_collapse_plan(hidden->plan, src, std::span{hidden_pose}.first(n));
            ctx.body_hider.note_applied();
            targets.skin_set_pose(object, edx, hidden_pose.data(), count);
            return;
        }
        // A part of the player's model posed from another skeleton's pose
        // (the mapped pose of 21 §2.3) keeps it, and that is reported.
        if (!players && game::hands::is_model_object(
                            game::CodeAddress{static_cast<std::uint32_t>(hidden->animation)}, object))
            ctx.body_hider.note_other_pose();
    }
    targets.skin_set_pose(object, edx, pose, count);
}

} // namespace omvr::loader::hooks
