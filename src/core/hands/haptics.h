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

// The game pad's two motors (21 §2.9): RumbleSetMotors' first and second
// level.
enum class PadMotor : std::uint8_t { Low, High };

// 21 §2.9, §14.4 item 1: the motor speed, as a fraction 0 to 1 of full
// speed, that the game's pad curve gives `level` (0 to 1; NaN gives 0).
[[nodiscard]] float pad_motor_speed(float level, PadMotor motor) noexcept;

// 21 §14.4 items 1 to 3: the amplitude a game rumble level is played at
// (before Hands.Haptics.Strength): the motor speed squared, 0 below the
// floor.
[[nodiscard]] float rumble_amplitude(float level, PadMotor motor) noexcept;

// 21 §18 (r11): the game's rumble since the last debug line.
struct RumbleWindow {
    float highest_low    = 0.0f; // the highest level received per channel
    float highest_high   = 0.0f;
    std::uint32_t calls  = 0;    // RumbleSetMotors calls
    float highest_played = 0.0f; // the highest amplitude played, after §14.4 and Strength
};

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

    // The tracker scan's thread: a scan ran. `contact` is its result (a
    // contact within the tracker's range), `raised` the tracker's raised
    // byte; only both tick (21 §14.4 item 6, Hands.Haptics.Tracker).
    void on_tracker_scan(bool contact, bool raised) noexcept;

    // Any thread: the game set its pad's motors (0 to 1). The low-frequency
    // motor goes to the left hand, the high-frequency one to the right,
    // each held until the next call (21 §14.4; Hands.Haptics.GameRumble,
    // read on the render thread).
    void on_rumble(float low, float high, TimePoint now) noexcept;

    // Any thread but the render thread, every 900 frames with Dev. Debug
    // Options (21 §18): logs the rumble received and played since the last
    // call, when any level was not 0 in that time, and starts a new window.
    void log_rumble_window() noexcept;

    // The window log_rumble_window logs, taken and restarted (for tests and
    // the log line).
    RumbleWindow take_rumble_window() noexcept;

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
    std::array<float, kHandCount> rumble_amplitudes(TimePoint now) const noexcept;
    bool rumble_held(TimePoint now) const noexcept;
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
    // The debug window (21 §18). Relaxed: values for a log line.
    std::atomic<float> window_low_{0.0f};
    std::atomic<float> window_high_{0.0f};
    std::atomic<std::uint32_t> window_calls_{0};
    std::atomic<float> window_played_{0.0f}; // render thread writes

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
