// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/input/gesture_geometry.h"
#include "core/input/ports/gesture_context.h"
#include "core/input/types.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

// The body gestures of 22 §8.2 (two-handed recenter), §8.4 (holsters) and
// §9 (lean and hold breath while hiding), over the head and hand poses of
// each controls write (22 §8.1). The control mapper hands each write in and
// applies the result to the game's action tiers; the recenter is carried
// out on the game thread (22 §15).
namespace omvr::core::input {

// 22 §8.2. Both grips held with both hands near the head for kRecenterHold
// fires once; the hold restarts whenever a condition breaks, and after
// firing the grips or the hands must leave first. Pure state; render thread.
class RecenterGesture {
public:
    struct Input {
        bool grips = false; // both grips pressed
        bool near  = false; // both hands within kRecenterHandDistance of the head
        bool fire  = false; // Fire pressed: never while it is
        bool open  = false; // the focus gate of 22 §8.1
    };
    enum class Result : std::uint8_t {
        None,
        Fired,
        SuppressedFire,  // the gesture is held with Fire pressed
        SuppressedFocus, // the gesture is held without input focus, or just after it returned
    };

    // `enabled`: Controls.RecenterGesture. Each suppression is reported once
    // per performance.
    Result update(bool enabled, const Input& in, TimePoint now) noexcept;

private:
    std::optional<TimePoint> since_; // the conditions have held since
    bool fired_      = false;        // fired; waits for the grips or hands to leave
    bool suppressed_ = false;        // a suppression was reported for this performance
};

// The holster zones of 22 §8.4, each driving one map 0 action.
enum class Holster : std::uint8_t { None, RightShoulder, LeftShoulder, LeftHip, Chest };

// The zone a hand at head-frame position `p` is in, for hand `hand`.
[[nodiscard]] Holster holster_zone(std::size_t hand, const Vec3& p) noexcept;

// 22 §8.4. A grip press that begins in a zone holds that zone's action
// until the grip is released, and the grip is consumed meanwhile (it drives
// neither Aim nor the tracker). Pure state; render thread.
class Holsters {
public:
    struct HandInput {
        bool grip = false;
        std::optional<Vec3> position; // head frame; nullopt without valid poses
    };
    struct Input {
        std::array<HandInput, 2> hands{}; // left, right
        bool blocked = false;             // an interaction or a menu (mode 1)
        bool open    = false;             // the focus gate of 22 §8.1
    };
    struct Event {
        std::size_t hand = 0;
        Holster zone     = Holster::None;
        bool fired       = false; // a press began in this zone: fired, or suppressed when false
    };
    struct Result {
        std::array<Holster, 2> held{};  // the zone each hand holds (None: not holding)
        std::array<bool, 2> consumed{}; // the grip belongs to a holster until released
        std::optional<Event> event;     // a press that began in a zone this write
    };

    // `enabled`: Controls.HolsterGestures.
    Result update(bool enabled, const Input& in) noexcept;

private:
    std::array<bool, 2> prev_grip_{};
    std::array<Holster, 2> held_{};
    std::array<bool, 2> consumed_{};
};

// One axis of the lean (22 §9.2): starts beyond kLeanStart, stops inside
// kLeanStop, in proportion up to kLeanFull.
class LeanAxis {
public:
    // The lean along this axis, -1 to 1, for the offset `v` (metres).
    float update(float v) noexcept;
    void reset() noexcept { active_ = false; }

private:
    bool active_ = false;
};

// 22 §9.2. While hiding and not in a menu, the head's offset from where it
// was when hiding began (head frame of that moment) leans the character
// through the movement axes. Pure state; render thread.
class HidingLean {
public:
    struct Input {
        bool hiding            = false;
        bool menu              = false;
        bool open              = false; // the focus gate of 22 §8.1
        const BodyPoses* poses = nullptr;
    };
    struct Result {
        Vec2 move;                    // added to the movement axes (x right, y forward)
        bool reference_taken = false; // the reference was taken this write
    };

    // `enabled`: Controls.HidingLean.
    Result update(bool enabled, const Input& in) noexcept;

private:
    void reset() noexcept;

    std::optional<HeadFrame> reference_;
    LeanAxis side_, forward_, up_;
};

// 22 §9.3. While the breathing game is in progress, either hand over the
// mouth point holds the breath (presses the game's hold-breath input).
// Pure state; render thread.
class HoldBreath {
public:
    struct Input {
        bool breathing_game    = false;
        bool menu              = false;
        bool open              = false; // the focus gate of 22 §8.1
        const BodyPoses* poses = nullptr;
    };

    // `enabled`: Controls.HoldBreathGesture. True while the breath is held.
    bool update(bool enabled, const Input& in) noexcept;

private:
    bool held_ = false;
};

// What the gestures do to one controls write.
struct GestureResult {
    std::array<bool, 2> grip_consumed{}; // left, right: by a holster (22 §8.4)
    bool next_weapon = false;            // map 0 next_main_weapon, held by the right shoulder
    bool prev_weapon = false;            // map 0 prev_main_weapon, held by the left shoulder
    bool torch       = false;            // map 0 select_torch, held by the left hip
    bool tracker     = false;            // map 0 select_motion_tracker, held by the chest
    Vec2 lean;                           // added to the movement axes (22 §9.2)
    bool hold_breath = false;            // presses the game's hold-breath input (22 §9.3)
};

struct GesturesDeps {
    const config::ControlsSettings& controls; // read on every write: a menu change applies at once
    const ports::GestureContext& context;
    diag::ports::Log& log;
};

// The gestures over each controls write. update on the render thread;
// take_recenter on the game thread.
class Gestures final {
public:
    explicit Gestures(const GesturesDeps& deps) noexcept
        : controls_(deps.controls), context_(deps.context), log_(deps.log) {}

    // One controls write: `c` as read, with its poses; `now` its time.
    // Called with every gesture off too, so each one's state resets.
    GestureResult update(const Controls& c, TimePoint now) noexcept;

    // A recenter the gesture asked for since the last call (22 §8.2, §15).
    // Game thread.
    bool take_recenter() noexcept { return recenter_request_.exchange(false, std::memory_order_acq_rel); }

private:
    void run_recenter(const Controls& c, bool open, TimePoint now) noexcept;
    void apply_holsters(const Controls& c, const ports::GameContext& game, bool open,
                        GestureResult& out) noexcept;
    void apply_lean(const Controls& c, const ports::GameContext& game, bool open,
                    GestureResult& out) noexcept;
    void apply_breath(const Controls& c, const ports::GameContext& game, bool open,
                      GestureResult& out) noexcept;

    const config::ControlsSettings& controls_;
    const ports::GestureContext& context_;
    diag::ports::Log& log_;

    // Render thread.
    FocusGate gate_;
    RecenterGesture recenter_;
    Holsters holsters_;
    HidingLean lean_;
    HoldBreath breath_;
    bool lean_logged_   = false; // the lean last logged as active
    bool breath_logged_ = false; // the breath last logged as held

    std::atomic<bool> recenter_request_{false};
};

} // namespace omvr::core::input
