#include "core/stereo/stereo_aa.h"

#include "core/config/settings_table.h"

#include <array>
#include <string_view>
#include <utility>

namespace omvr::core::stereo {

namespace {

// Indexed by AaMode and AaAction.
constexpr std::array<std::string_view, 4> kModeNames   = {"off", "FXAA", "SMAA T1x", "SMAA T2x"};
constexpr std::array<std::string_view, 3> kActionNames = {"skipped", "runs it", "runs it as SMAA T1x"};
constexpr std::string_view kUnknownMode                = "unknown";
constexpr std::string_view kNothingToRun               = "nothing to run";
constexpr std::string_view kDecisionLog = "[stereo] Stereo anti-aliasing for game mode {}: {} (setting {})";

bool is_aa_mode(AaMode mode) noexcept { return mode >= AaMode::Fxaa && mode <= AaMode::SmaaT2x; }

bool is_known_mode(AaMode mode) noexcept { return mode >= AaMode::Off && mode <= AaMode::SmaaT2x; }

AaAction action_of(AaMode mode, bool run) noexcept {
    if (!run) return AaAction::Skip;
    return mode == AaMode::SmaaT2x ? AaAction::RunAsT1x : AaAction::Run;
}

} // namespace

bool StereoAa::runs_aa_pass(AaMode mode) const noexcept {
    return is_aa_mode(mode) && config::stereo_aa_enabled(settings_.rendering);
}

void StereoAa::log_decision(AaMode mode, bool run) noexcept {
    const std::int32_t mode_id = std::to_underlying(mode);
    const int action           = std::to_underlying(action_of(mode, run));
    if (mode_id == logged_mode_ && action == logged_action_) return;
    logged_mode_   = mode_id;
    logged_action_ = action;
    const std::string_view name =
        is_known_mode(mode) ? kModeNames[static_cast<std::size_t>(mode_id)] : kUnknownMode;
    diag::ports::logf(log_, kDecisionLog, name,
                      mode == AaMode::Off ? kNothingToRun : kActionNames[static_cast<std::size_t>(action)],
                      config::to_string(settings_.rendering.stereo_aa));
}

} // namespace omvr::core::stereo
