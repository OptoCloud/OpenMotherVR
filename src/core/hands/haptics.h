// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/clock.h"
#include "core/diag/ports/log.h"
#include "core/hands/tuning.h"
#include "core/hands/types.h"
#include "core/input/interaction_state.h"
#include "core/vr/ports/haptic_source.h"
#include "game/constants.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

// The controllers' vibration for the game's feedback (spec 21 §14).
//
// The game's sources are recorded where they happen (21 §14.3, §17): the
// health and the fire control on the game thread at each head pose, the
// tracker scan on its thread, the start and end of hacking from the script
// event tap, the game's rumble on whichever thread sets it. Each event is
// one atomic sequence number with its pulse, so nothing waits; the render
// thread turns them into each hand's output once per frame (next), right
// after the runtime reads the controls, so only the render thread calls
// the runtime.
namespace omvr::core::hands {

// The events of 21 §14.2 that have a verified game source.
enum class HapticEvent : std::uint8_t { Damage, Death, TrackerPing, Firing, Hacking };
inline constexpr std::size_t kHapticEventCount = 5;

// One pulse: amplitude 0 to 1 before Hands.Haptics.Strength.
struct Pulse {
    float amplitude = 0.0f;
    Seconds duration{};
};

// 21 §14.2: the damage pulse for health falling from `before` to `after`
// of `maximum`.
[[nodiscard]] float damage_amplitude(std::uint16_t before, std::uint16_t after,
                                     std::uint16_t maximum) noexcept;

class Haptics final : public vr::ports::HapticSource {
public:
    Haptics(const config::HapticsSettings& settings, const diag::ports::Clock& clock,
            diag::ports::Log& log) noexcept
        : settings_(settings), clock_(clock), log_(log) {}

    // Game thread, at each head pose: the player's health, nullopt without a
    // player. A fall pulses damage, a fall to 0 death (Hands.Haptics.Damage).
    void on_health(const std::optional<Health>& health) noexcept;

    // Game thread, at each head pose: a shot on the press of the fire
    // control; the flamethrower pulses again as each pulse ends while it is
    // held (Hands.Haptics.Firing).
    void on_fire(const FireSample& fire, TimePoint now) noexcept;

    // Game thread (the script event tap): the interaction in progress
    // (None: none); the start and the end of hacking or cutting pulse
    // (Hands.Haptics.Hacking).
    void on_interaction(input::Interaction kind) noexcept;

    // The tracker scan's thread: a scan ran; `raised` is the tracker held up
    // (Hands.Haptics.Tracker).
    void on_tracker_scan(bool raised) noexcept;

    // Any thread: the game set its pad's motors (0 to 1). The low-frequency
    // motor goes to the left hand, the high-frequency one to the right
    // (Hands.Haptics.GameRumble, read on the render thread).
    void on_rumble(float low, float high, TimePoint now) noexcept;

    // Render thread, once per frame.
    vr::ports::HapticFrame next(bool focus, const std::array<bool, kHandCount>& tracked) noexcept override;

private:
    struct Channel {
        std::atomic<std::uint32_t> sequence{0};
        std::atomic<float> amplitude{0.0f};
        std::atomic<float> seconds{0.0f};
    };
    // Render thread: one hand's pulse of each event, and what it plays.
    struct Slot {
        float amplitude = 0.0f;
        TimePoint end{};
    };
    struct HandState {
        std::array<Slot, kHapticEventCount> slots{};
        bool rumble_on = false;
        TimePoint playing_until{};
    };

    void emit(HapticEvent e, Pulse p) noexcept;
    std::array<bool, kHandCount> take_events(TimePoint now) noexcept;
    std::array<float, kHandCount> rumble_levels(TimePoint now) const noexcept;
    vr::ports::HapticHand plan_hand(std::size_t h, TimePoint now, bool started, float rumble, float strength,
                                    bool can_play) noexcept;

    const config::HapticsSettings& settings_;
    const diag::ports::Clock& clock_;
    diag::ports::Log& log_;

    // Written by the events' threads, read on the render thread.
    std::array<Channel, kHapticEventCount> channels_{};
    std::atomic<float> rumble_low_{0.0f};
    std::atomic<float> rumble_high_{0.0f};
    util::AtomicStamp rumble_stamp_; // the last set; none: never

    // Game thread.
    std::optional<Health> health_;
    bool fire_pressed_ = false;
    std::optional<TimePoint> flame_until_;
    bool hacking_ = false;

    // Render thread.
    std::array<std::uint32_t, kHapticEventCount> seen_{};
    std::array<HandState, kHandCount> hands_{};
};

} // namespace omvr::core::hands
