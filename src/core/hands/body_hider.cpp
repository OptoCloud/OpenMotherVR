// OpenMotherVR-Provenance: clean
#include "core/hands/body_hider.h"

#include "core/config/settings_table.h"
#include "core/hands/body_tuning.h"

#include <algorithm>
#include <format>

namespace omvr::core::hands {

namespace {

using config::BodyVisible;
using diag::ports::logf;

constexpr std::string_view kLogSkeleton =
    "[body] player skeleton {}: {} bones; LeftShoulder {}, RightShoulder {}, LeftArm {}, RightArm {}, "
    "LeftHand {}, RightHand {}, Spine {}";
constexpr std::string_view kLogMissing =
    "[body] WARNING player skeleton {}: no bone {} by name; the body is drawn whole with it (21 §10.2)";
constexpr std::string_view kLogTooMany =
    "[body] WARNING player skeleton {}: {} bones, more than the {} body visibility handles; the body is "
    "drawn whole with it";
constexpr std::string_view kLogBadParents =
    "[body] WARNING player skeleton {}: its parent indices are not a tree; the body is drawn whole with it";
constexpr std::string_view kLogUnreadable =
    "[body] WARNING the player's skeleton {} could not be read ({}); the body is drawn whole until it "
    "can be, read again every {:.0f} s";
constexpr std::string_view kLogHidden = "[body] visibility {}: {} of {} bones of {} collapsed";
constexpr std::string_view kLogWhole  = "[body] visibility Full: the whole body is drawn";
constexpr std::string_view kLogApplied =
    "[body] the player's model is drawn from the pose with the hidden bones collapsed";
constexpr std::string_view kLogOtherPose =
    "[body] WARNING a part of the player's model is posed from another skeleton's pose and stays drawn "
    "(21 §22 item 27)";

// The fault as the log says it.
std::string_view fault_text(SkeletonFault f) noexcept {
    switch (f) {
        case SkeletonFault::Unknown: return "no reason given";
        case SkeletonFault::NoBones: return "no bones";
        case SkeletonFault::NoArrays: return "no bone or parent array";
        case SkeletonFault::TooManyBones: return "too many bones";
        case SkeletonFault::UnnamedBone: return "a bone without a name";
    }
    return "?";
}

// The index of a bone by its name without prefix, -1 when absent.
int index_of(const SkeletonBones& b, std::string_view name) noexcept {
    for (std::size_t i = 0; i < b.names.size(); ++i)
        if (bare_bone_name(b.names[i]) == name) return static_cast<int>(i);
    return -1;
}

} // namespace

bool BodyHider::wants_bones(BodyVisible v, const PlayerSkeleton& s, BodyTime now) const noexcept {
    if (v == BodyVisible::Full || s.pose == 0) return false;
    if (!built_any_ || v != built_value_ || s.skeleton != built_skeleton_ || s.pose != built_pose_)
        return true;
    return retry_at_ && now >= *retry_at_;
}

void BodyHider::update(BodyVisible v, bool running, const PlayerSkeleton& s, const SkeletonBones* bones,
                       BodyTime now, const SkeletonReadFault& fault) noexcept {
    log_job_reports();
    if (v == BodyVisible::Full && built_any_ && built_value_ != BodyVisible::Full) {
        log_.line(kLogWhole);
        built_value_ = BodyVisible::Full;
    }
    if (!running || v == BodyVisible::Full || s.pose == 0) {
        current_.store(nullptr, std::memory_order_release);
        return;
    }
    if (wants_bones(v, s, now)) {
        const bool changed =
            !built_any_ || v != built_value_ || s.skeleton != built_skeleton_ || s.pose != built_pose_;
        if (changed) unreadable_logged_.reset();
        built_any_      = true;
        built_value_    = v;
        built_skeleton_ = s.skeleton;
        built_pose_     = s.pose;
        built_ok_       = false;
        retry_at_.reset();
        if (bones) {
            build(v, s, *bones);
        } else {
            // 21 §10.2: not ready yet (a menu, a level load) or not readable
            // at all: read again until it is, logged once per change.
            retry_at_ = now + std::chrono::duration_cast<BodyTime::duration>(kSkeletonRetry);
            log_unreadable(s, fault);
        }
    }
    current_.store(built_ok_ ? &buffers_[next_ ^ 1U] : nullptr, std::memory_order_release);
}

void BodyHider::log_unreadable(const PlayerSkeleton& s, const SkeletonReadFault& fault) noexcept {
    if (unreadable_logged_ == fault.fault) return;
    unreadable_logged_ = fault.fault;
    // A fixed buffer: the game thread logs without allocating here.
    std::array<char, 96> why{};
    const auto text = fault_text(fault.fault);
    const auto r =
        fault.fault == SkeletonFault::UnnamedBone
            ? std::format_to_n(why.data(), why.size(), "{}: bone {} of {}", text, fault.bone, fault.count)
            : std::format_to_n(why.data(), why.size(), "{}, {} bones", text, fault.count);
    const std::string_view reason{why.data(), static_cast<std::size_t>(r.out - why.data())};
    logf(log_, kLogUnreadable, s.name.empty() ? std::string_view{"(no name)"} : s.name, reason,
         kSkeletonRetry.count());
}

void BodyHider::build(BodyVisible v, const PlayerSkeleton& s, const SkeletonBones& bones) noexcept {
    // The buffer no job reads: the published one is the other (21 §17).
    HiddenPose& out   = buffers_[next_];
    const auto result = build_collapse_plan(bones, v, out.plan);
    log_skeleton(s, bones, result);
    if (result != PlanResult::Ok) return;
    out.animation = s.animation;
    out.pose      = s.pose;
    out.skeleton  = s.skeleton;
    built_ok_     = true;
    next_ ^= 1U;
    logf(log_, kLogHidden, config::to_string(v), out.plan.hidden, out.plan.count, s.name);
}

void BodyHider::log_skeleton(const PlayerSkeleton& s, const SkeletonBones& bones, PlanResult r) noexcept {
    const auto seen = std::span{logged_skeletons_}.first(logged_count_);
    if (std::ranges::find(seen, s.skeleton) != seen.end()) return;
    if (logged_count_ < logged_skeletons_.size()) logged_skeletons_[logged_count_++] = s.skeleton;
    switch (r) {
        case PlanResult::Ok:
            logf(log_, kLogSkeleton, s.name, bones.names.size(), index_of(bones, "LeftShoulder"),
                 index_of(bones, "RightShoulder"), index_of(bones, "LeftArm"), index_of(bones, "RightArm"),
                 index_of(bones, "LeftHand"), index_of(bones, "RightHand"), index_of(bones, "Spine"));
            break;
        case PlanResult::MissingBones: logf(log_, kLogMissing, s.name, first_missing_bone(bones)); break;
        case PlanResult::TooManyBones:
            logf(log_, kLogTooMany, s.name, bones.names.size(), kMaxBodyBones);
            break;
        case PlanResult::BadParents: logf(log_, kLogBadParents, s.name); break;
    }
}

void BodyHider::log_job_reports() noexcept {
    if (!applied_logged_ && applied_.load(std::memory_order_relaxed)) {
        applied_logged_ = true;
        log_.line(kLogApplied);
    }
    if (!other_pose_logged_ && other_pose_.load(std::memory_order_relaxed)) {
        other_pose_logged_ = true;
        log_.line(kLogOtherPose);
    }
}

} // namespace omvr::core::hands
