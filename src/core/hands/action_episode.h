// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/types.h"
#include "util/time/steady.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

// The player action episode of spec 21 §12.1 (r11): a grab, a trap, a
// melee kill or a hit takes the body from the player from the start of
// the game's action until the player takes up an item, the first-person
// camera has been back for a moment, or a safety time has passed.
namespace omvr::core::hands {

// The scripted player actions whose start methods are hooked (21 §2.11).
enum class PlayerAction : std::uint8_t { MeleeKill, TrapBreak, MeleeIntercept, ImpactStanding, TrapVictim };
inline constexpr std::size_t kPlayerActionCount = 5;

// The action's name in the game (21 §2.11).
std::string_view action_name(PlayerAction a) noexcept;

// Why an episode ended (21 §12.1).
enum class EpisodeEnd : std::uint8_t { ItemTaken, CameraBack, Safety };
std::string_view to_string(EpisodeEnd e) noexcept;

class ActionEpisode final {
public:
    // Any thread (the action's start method): an action started for the
    // player [likely: they are player actions by name, 21 §12.1].
    void on_start(PlayerAction a, TimePoint now) noexcept;

    // An episode runs or a start has not been taken up yet: the current
    // item is worth reading for update(). Game thread.
    bool pending_or_running() const noexcept;

    // What update() saw change.
    struct Change {
        std::optional<PlayerAction> started; // an episode began (or began again) with this action
        std::optional<EpisodeEnd> ended;     // the episode ended, and why
    };

    // Game thread, each frame (called by the hands and the body; a repeated
    // call in one frame changes nothing): `item` identifies the current
    // item (0: none), `follow_active` the first-person camera. Returns what
    // changed; running() then says whether the episode goes on.
    Change update(std::uintptr_t item, bool follow_active, TimePoint now) noexcept;

    bool running() const noexcept { return episode_.has_value(); }

private:
    struct Episode {
        PlayerAction kind = PlayerAction::MeleeKill;
        TimePoint start{};
        std::uintptr_t item = 0;     // the last item seen
        bool camera_lost    = false; // the camera was taken during the episode
        std::optional<TimePoint> camera_back;
    };
    std::optional<EpisodeEnd> end_reason(Episode& e, std::uintptr_t item, bool follow_active,
                                         TimePoint now) const noexcept;

    // Written by the start methods' thread, read on the game thread.
    std::atomic<std::uint32_t> starts_{0};
    std::atomic<std::uint8_t> kind_{0};
    util::AtomicStamp started_at_;

    // Game thread.
    std::uint32_t seen_ = 0;
    std::optional<Episode> episode_;
};

} // namespace omvr::core::hands
