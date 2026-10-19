// OpenMotherVR-Provenance: clean
// Interaction.CameraTakeovers' hooks (spec 23 §4.2 (b), (c)).

#include "loader/hooks/camera_hooks.h"

#include "core/config/settings_table.h"
#include "core/ui/camera_takeovers.h"
#include "game/patcher.h"
#include "game/sites/camera_takeovers.h"
#include "loader/context.h"
#include "loader/hooks/guard.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

namespace sites = game::sites;
using core::diag::ports::logf;
using core::ui::Takeover;

// 23 §4.2, §10, acceptance 5: part (a) waits for 23 §12 items 5 and 12;
// said once when the key first asks for "Reduced".
constexpr std::string_view kLogPartA =
    "interaction: Camera Takeovers Reduced: scripted turns and the re-align to the body are not applied; "
    "the view still follows the short device animations in this build: how the game identifies a "
    "request's animation and which animations are short device ones are not known yet (23 §4.2 (a), "
    "§12 items 5 and 12)";
constexpr std::string_view kLogSuppressed = "[diag] camera takeover: {} not applied ({})";
constexpr std::string_view kLogKept       = "[diag] camera takeover: {} kept ({})";

// The game image as the installer saw it, for an install once the key
// asks; whether the install was tried and went in; whether part (a)'s
// note was given. Game thread (attach runs before the game's threads).
std::optional<game::GameImage> image;
bool sites_tried  = false;
bool sites_on     = false;
bool part_a_noted = false;

bool reduced_asked(const Context& ctx) noexcept {
    return ctx.settings.current().interaction.camera_takeovers == core::config::CameraTakeovers::Reduced;
}

core::ui::TakeoverInputs inputs(const Context& ctx) noexcept {
    return {.setting          = ctx.settings.current().interaction.camera_takeovers,
            .in_interaction   = ctx.interactions.in_interaction(),
            .cutscene_or_fade = ctx.scene.cutscene_or_fade()};
}

// Whether the takeover at this call is to be skipped; with DebugOptions,
// each decision of a takeover the key covers is logged (23 §10). These
// calls are scripted events, not per frame.
bool skip(Context& ctx, Takeover kind) noexcept {
    // 23 (intro): with a failed runtime none of it runs.
    if (!sites_on || ctx.runtime_state.failed()) return false;
    const auto in       = inputs(ctx);
    const bool suppress = ctx.camera_takeovers.suppress(in);
    if (ctx.log.debug_enabled()) {
        const auto rule = core::config::to_string(ctx.camera_takeovers.rule(in));
        if (suppress)
            logf(ctx.log, kLogSuppressed, core::ui::takeover_name(kind), rule);
        else if (in.cutscene_or_fade)
            logf(ctx.log, kLogKept, core::ui::takeover_name(kind), "cutscene or fade");
        else if (in.setting != core::config::CameraTakeovers::Game)
            logf(ctx.log, kLogKept, core::ui::takeover_name(kind), rule);
    }
    return suppress;
}

// FollowSteer at the call of `kKind` (thiscall on the follow camera, ret
// 0x10). Skipped, the follow camera keeps its yaw, pitch, target and
// steering flags: 23 §4.2 (b).
template <Takeover kKind>
void __fastcall steer(void* follow, void* edx, std::int32_t mode, const float* target, std::uint32_t flag,
                      float blend_time) noexcept {
    auto& ctx = context();
    if (guarded("FollowSteer", false, [&] { return skip(ctx, kKind); })) return;
    ctx.camera_targets.steer(follow, edx, mode, target, flag, blend_time);
}

// The follow camera's re-align to the body at the call of `kKind`
// (thiscall, ret 4). Skipped, the camera's heading reference stays as it
// was, so the view's yaw does not snap; the pitch levelling and steering
// clear after it still run: 23 §4.2 (c).
template <Takeover kKind>
void __fastcall realign(void* follow, void* edx, std::uint32_t at_once) noexcept {
    auto& ctx = context();
    if (guarded("FollowRealign", false, [&] { return skip(ctx, kKind); })) return;
    ctx.camera_targets.realign(follow, edx, at_once);
}

constexpr auto* steal_camera_steer        = &steer<Takeover::StealCameraSteer>;
constexpr auto* level_camera_exit_steer   = &steer<Takeover::LevelCameraExitSteer>;
constexpr auto* level_camera_exit_realign = &realign<Takeover::LevelCameraExitRealign>;
constexpr auto* reset_realign             = &realign<Takeover::ResetRealign>;
constexpr auto* state_entry_realign       = &realign<Takeover::StateEntryRealign>;

static_assert(std::is_convertible_v<decltype(steal_camera_steer), sites::FollowSteerFn>);
static_assert(std::is_convertible_v<decltype(level_camera_exit_steer), sites::FollowSteerFn>);
static_assert(std::is_convertible_v<decltype(level_camera_exit_realign), sites::FollowRealignFn>);
static_assert(std::is_convertible_v<decltype(reset_realign), sites::FollowRealignFn>);
static_assert(std::is_convertible_v<decltype(state_entry_realign), sites::FollowRealignFn>);

constexpr sites::CameraTakeoverHooks kHooks{steal_camera_steer, level_camera_exit_steer,
                                            level_camera_exit_realign, reset_realign, state_entry_realign};

void ensure_sites(Context& ctx) noexcept {
    if (!image || !reduced_asked(ctx)) return;
    if (!part_a_noted) {
        part_a_noted = true;
        ctx.log.line(kLogPartA);
    }
    if (sites_tried) return;
    sites_tried = true;
    game::Patcher patcher{ctx.log};
    sites_on = sites::install_camera_takeover_sites(*image, patcher, kHooks, ctx.camera_targets);
}

} // namespace

void prepare_camera_takeovers(const game::GameImage& g) noexcept {
    image.emplace(g);
    ensure_sites(context());
}

void camera_takeovers_on_stereo_update() noexcept {
    auto& ctx = context();
    guarded("camera takeovers", [&] {
        ensure_sites(ctx);
        ctx.camera_takeovers.observe(inputs(ctx));
    });
}

} // namespace omvr::loader::hooks
