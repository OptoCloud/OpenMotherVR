// OpenMotherVR-Provenance: clean
#include "core/hands/action_episode.h"

#include "core/hands/tuning.h"

#include <utility>

namespace omvr::core::hands {

std::string_view action_name(PlayerAction a) noexcept {
    switch (a) {
        case PlayerAction::MeleeKill: return "ACT_PLAYER_MELEE_KILL";
        case PlayerAction::TrapBreak: return "ACT_PLAYER_TRAP_BREAK";
        case PlayerAction::MeleeIntercept: return "ACT_PLAYER_MELEE_INTERCEPT";
        case PlayerAction::ImpactStanding: return "ACT_PLAYER_IMPACT_STANDING";
        case PlayerAction::TrapVictim: return "ACT_PLAYER_TRAP_VICTIM";
    }
    return "?";
}

std::string_view to_string(EpisodeEnd e) noexcept {
    switch (e) {
        case EpisodeEnd::ItemTaken: return "an item taken up";
        case EpisodeEnd::CameraBack: return "the first-person camera back";
        case EpisodeEnd::Safety: return "the safety time";
    }
    return "?";
}

void ActionEpisode::on_start(PlayerAction a, TimePoint now) noexcept {
    kind_.store(std::to_underlying(a), std::memory_order_relaxed);
    started_at_.store(now);
    starts_.fetch_add(1, std::memory_order_release); // publishes the kind and time above
}

bool ActionEpisode::pending_or_running() const noexcept {
    return episode_ || starts_.load(std::memory_order_acquire) != seen_;
}

// 21 §12.1 (r11): the first of the current item changing from none to an
// item or from one item to another; kActionCameraSettle after the camera,
// taken during the episode, is back; kActionSafety after the start.
std::optional<EpisodeEnd> ActionEpisode::end_reason(Episode& e, std::uintptr_t item, bool follow_active,
                                                    TimePoint now) const noexcept {
    const std::uintptr_t before = std::exchange(e.item, item);
    if (item != 0 && item != before) return EpisodeEnd::ItemTaken;
    if (!follow_active) {
        e.camera_lost = true;
        e.camera_back.reset();
    } else if (e.camera_lost) {
        if (!e.camera_back) e.camera_back = now;
        if (now >= util::after(*e.camera_back, kActionCameraSettle)) return EpisodeEnd::CameraBack;
    }
    if (now >= util::after(e.start, kActionSafety)) return EpisodeEnd::Safety;
    return std::nullopt;
}

ActionEpisode::Change ActionEpisode::update(std::uintptr_t item, bool follow_active, TimePoint now) noexcept {
    Change c;
    const std::uint32_t starts = starts_.load(std::memory_order_acquire);
    if (starts != seen_) {
        seen_            = starts;
        const auto kind  = static_cast<PlayerAction>(kind_.load(std::memory_order_relaxed));
        const auto start = started_at_.load();
        // The item held as the action starts is not one taken up after it.
        episode_  = Episode{kind, start.value_or(now), item, !follow_active, std::nullopt};
        c.started = kind;
        return c;
    }
    if (!episode_) return c;
    c.ended = end_reason(*episode_, item, follow_active, now);
    if (c.ended) episode_.reset();
    return c;
}

} // namespace omvr::core::hands
