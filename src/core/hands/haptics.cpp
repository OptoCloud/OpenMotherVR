// OpenMotherVR-Provenance: clean
#include "core/hands/haptics.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace omvr::core::hands {

namespace {

using diag::ports::logf;
using util::after;
using vr::ports::HapticHand;

constexpr std::string_view kLogPulse = "[diag] haptics: {} pulse, amplitude {:.2f}, {:.0f} ms";

constexpr float kMillisecondsPerSecond = 1000.0f;

// 21 §14.2: which hands each event vibrates (left, right).
constexpr std::array<std::array<bool, kHandCount>, kHapticEventCount> kEventHands = {{
    {true, true},  // damage
    {true, true},  // death
    {true, false}, // tracker ping: the hand holding the tracker
    {false, true}, // firing
    {true, true},  // hacking start, end
}};

const char* event_name(HapticEvent e) noexcept {
    switch (e) {
        case HapticEvent::Damage: return "damage";
        case HapticEvent::Death: return "death";
        case HapticEvent::TrackerPing: return "tracker";
        case HapticEvent::Firing: return "firing";
        case HapticEvent::Hacking: return "hacking";
    }
    return "?";
}

std::size_t slot(HapticEvent e) noexcept { return static_cast<std::size_t>(e); }

// A level from the game: 0 to 1, and 0 at or below the floor (or NaN).
float audible(float level) noexcept { return level > kRumbleFloor ? std::min(level, 1.0f) : 0.0f; }

} // namespace

float damage_amplitude(std::uint16_t before, std::uint16_t after, std::uint16_t maximum) noexcept {
    if (after >= before) return 0.0f;
    if (maximum == 0) return kHapticMaxAmplitude;
    const float lost = static_cast<float>(before - after) / static_cast<float>(maximum);
    return std::min(kDamageBase + kDamagePerLostMaximum * lost, kHapticMaxAmplitude);
}

bool ranged(game::WeaponType type) noexcept {
    using game::WeaponType;
    return type == WeaponType::Flamethrower || type == WeaponType::Pistol || type == WeaponType::Shotgun ||
           type == WeaponType::Boltgun;
}

// ---- Game side ------------------------------------------------------------------

void Haptics::emit(HapticEvent e, Pulse p) noexcept {
    Channel& c = channels_[slot(e)];
    c.amplitude.store(p.amplitude, std::memory_order_relaxed);
    c.seconds.store(p.duration.count(), std::memory_order_relaxed);
    c.sequence.fetch_add(1, std::memory_order_release); // publishes the pulse above
    // Dev. Debug Options: each event once as it happens (not the tracker's
    // ticks, twice a second). Never the render thread.
    if (e != HapticEvent::TrackerPing && log_.debug_enabled()) {
        logf(log_, kLogPulse, event_name(e), p.amplitude, p.duration.count() * kMillisecondsPerSecond);
    }
}

void Haptics::on_health(const std::optional<Health>& health) noexcept {
    const std::optional<Health> before = std::exchange(health_, health);
    if (!health || !before || before->player != health->player) return;
    if (health->current >= before->current || !settings_.damage) return;
    if (health->current == 0) {
        emit(HapticEvent::Death, {kDeathAmplitude, kDeathDuration});
        return;
    }
    emit(HapticEvent::Damage,
         {damage_amplitude(before->current, health->current, health->maximum), kDamageDuration});
}

void Haptics::on_fire(const FireSample& fire, TimePoint now) noexcept {
    const bool pressed_now = fire.pressed && !std::exchange(fire_pressed_, fire.pressed);
    const bool fires       = fire.pressed && fire.weapon && ranged(*fire.weapon) && fire.gate_open;
    if (!fires || !settings_.firing) {
        flame_until_.reset();
        return;
    }
    if (*fire.weapon == game::WeaponType::Flamethrower) {
        if (flame_until_ && now < *flame_until_) return;
        flame_until_ = after(now, kFlameDuration);
        emit(HapticEvent::Firing, {kFlameAmplitude, kFlameDuration});
        return;
    }
    flame_until_.reset();
    if (pressed_now) emit(HapticEvent::Firing, {kFiringAmplitude, kFiringDuration});
}

void Haptics::on_interaction(bool hacking_or_cutting) noexcept {
    if (std::exchange(hacking_, hacking_or_cutting) == hacking_or_cutting || !settings_.hacking) return;
    emit(HapticEvent::Hacking, {kHackingAmplitude, kHackingDuration});
}

void Haptics::on_tracker_scan(bool raised) noexcept {
    if (raised && settings_.tracker)
        emit(HapticEvent::TrackerPing, {kTrackerPingAmplitude, kTrackerPingDuration});
}

void Haptics::on_rumble(float low, float high, TimePoint now) noexcept {
    rumble_low_.store(low, std::memory_order_relaxed);
    rumble_high_.store(high, std::memory_order_relaxed);
    rumble_stamp_.store(now);
}

// ---- Render side ----------------------------------------------------------------

// The pulses the events' threads published since the last frame, into the
// hands' slots. Which hands got a new one.
std::array<bool, kHandCount> Haptics::take_events(TimePoint now) noexcept {
    std::array<bool, kHandCount> started{};
    for (std::size_t e = 0; e < kHapticEventCount; ++e) {
        const Channel& c         = channels_[e];
        const std::uint32_t seen = c.sequence.load(std::memory_order_acquire);
        if (seen == seen_[e]) continue;
        seen_[e] = seen;
        const Slot pulse{c.amplitude.load(std::memory_order_relaxed),
                         after(now, Seconds{c.seconds.load(std::memory_order_relaxed)})};
        for (std::size_t h = 0; h < kHandCount; ++h) {
            if (!kEventHands[e][h]) continue;
            hands_[h].slots[e] = pulse;
            started[h]         = true;
        }
    }
    return started;
}

// The game's rumble per hand (21 §14.2), 0 when off, stale or below the
// floor.
std::array<float, kHandCount> Haptics::rumble_levels(TimePoint now) const noexcept {
    const auto set = rumble_stamp_.load();
    if (!settings_.game_rumble || !set || now > util::after(*set, kRumbleStale)) return {};
    return {audible(rumble_low_.load(std::memory_order_relaxed)),
            audible(rumble_high_.load(std::memory_order_relaxed))};
}

vr::ports::HapticFrame Haptics::next(bool focus, const std::array<bool, kHandCount>& tracked) noexcept {
    const TimePoint now                        = clock_.now();
    const std::array<bool, kHandCount> started = take_events(now);
    const std::array<float, kHandCount> rumble = rumble_levels(now);
    const float strength = std::clamp(std::isnan(settings_.strength) ? 0.0f : settings_.strength, 0.0f, 1.0f);
    vr::ports::HapticFrame frame{};
    for (std::size_t h = 0; h < kHandCount; ++h) {
        frame[h] = plan_hand(h, now, started[h], rumble[h], strength, focus && tracked[h]);
    }
    return frame;
}

// 21 §14.3: one hand's output. The game's rumble is renewed every frame
// at the larger of its level and any pulse; without it a pulse is started
// once, for what is left of it. A hand that may not vibrate (no focus, lost)
// gets nothing, and anything still playing on it is stopped.
HapticHand Haptics::plan_hand(std::size_t h, TimePoint now, bool started, float rumble, float strength,
                              bool can_play) noexcept {
    HandState& s = hands_[h];
    Slot strongest;
    for (Slot& p : s.slots) {
        if (p.end <= now) p = {};
        if (p.amplitude > strongest.amplitude) strongest = p;
    }
    HapticHand out;
    if (!can_play) {
        // What came meanwhile is dropped, not played late.
        out.stop = now < s.playing_until;
        s        = {};
        return out;
    }
    const bool rumble_on = rumble > 0.0f;
    out.level            = std::max(rumble, strongest.amplitude) * strength;
    if (rumble_on) {
        out.play    = true;
        out.seconds = kRumbleRenewal.count();
    } else if (strongest.amplitude > 0.0f && (started || s.rumble_on)) {
        out.play    = true;
        out.seconds = Seconds{now < strongest.end ? strongest.end - now : TimePoint::duration{}}.count();
    } else if (s.rumble_on) {
        out.stop = true;
    }
    s.rumble_on   = rumble_on;
    out.amplitude = out.play ? out.level : 0.0f;
    out.play      = out.play && out.amplitude > 0.0f && out.seconds > 0.0f;
    if (out.play) s.playing_until = after(now, Seconds{out.seconds});
    if (out.stop) s.playing_until = {};
    return out;
}

} // namespace omvr::core::hands
