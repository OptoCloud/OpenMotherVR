#include "core/ui/commands.h"

#include "core/config/tuning.h"
#include "core/stereo/blinders.h"
#include "core/ui/runtime_menu.h"
#include "core/ui/ui_contract.h"
#include "util/text/number_parse.h"

#include <cstring>
#include <format>
#include <type_traits>
#include <utility>

namespace omvr::core::ui {

namespace {

using config::CommandEffect;
using config::CommandRule;
using config::SettingRow;
using Kind = CommandOutcome::Kind;

// Command tokens are separated by spaces; text after the value is ignored.
constexpr const char* kTokenSeparators = " ";

CommandOutcome refused() { return {Kind::Refused}; }

// Sets `field` to `value`; the change, under the row's key path, when it
// differs.
template <class T>
std::optional<SettingChange> assign(T& field, T value, const SettingRow& row, std::string text) {
    if (field == value) return std::nullopt;
    field = value;
    return SettingChange{row.path, std::move(text)};
}

// The row's field in `cfg`, as a T (the table pairs each rule with its type).
template <class T>
T& field_of(config::Settings& cfg, const SettingRow& row) {
    return std::get<config::Accessor<T>>(row.field)(cfg);
}

template <class T>
CommandOutcome applied(config::Settings& cfg, const SettingRow& row, T value, std::string text) {
    return {Kind::Applied, assign(field_of<T>(cfg, row), value, row, std::move(text)), row.effect};
}

CommandOutcome apply_switch(config::Settings& cfg, const SettingRow& row, const char* value) {
    const auto v = util::parse_switch(value);
    if (!v) return refused();
    return applied(cfg, row, *v, *v ? "true" : "false");
}

// 0 to the enum's last value; the enum's name in the log.
CommandOutcome apply_choice(config::Settings& cfg, const SettingRow& row, const char* value) {
    return std::visit(
        [&](auto field) -> CommandOutcome {
            using T = std::remove_reference_t<decltype(field(cfg))>;
            if constexpr (std::is_enum_v<T>) {
                const auto n = util::parse_int(value);
                if (!n || *n < 0 || *n > std::to_underlying(config::kLast<T>)) return refused();
                const auto e = static_cast<T>(*n);
                return applied(cfg, row, e, std::string(config::to_string(e)));
            } else {
                return refused();
            }
        },
        row.field);
}

// The range is the guard at FUN_100202e0 lines 183-185, which reset an
// out-of-range value to 1.0; here it is refused.
CommandOutcome apply_pixel_scale(config::Settings& cfg, const SettingRow& row, const char* value) {
    const auto v = util::parse_float(value);
    if (!v || !(*v >= config::kPixelScaleMin && *v <= config::kPixelScaleMax)) return refused();
    return applied(cfg, row, *v, std::format("{:.2f}", *v));
}

// At least kSnapSegmentsMinCommand, then rounded down to a step.
CommandOutcome apply_segments(config::Settings& cfg, const SettingRow& row, const char* value) {
    const auto n = util::parse_int(value);
    if (!n || *n < config::kSnapSegmentsMinCommand) return refused();
    const int segments = config::snap_segments(*n);
    return applied(cfg, row, segments, std::to_string(segments));
}

// A decimal number in the row's range (spec 22 §3.3); the log shows it
// with two decimals, as the pixel scale.
CommandOutcome apply_number(config::Settings& cfg, const SettingRow& row, const char* value) {
    const auto v = util::parse_float(value);
    if (!v || !row.range.accepts(*v)) return refused();
    return applied(cfg, row, *v, std::format("{:.2f}", *v));
}

CommandOutcome apply_integer(config::Settings& cfg, const SettingRow& row, const char* value) {
    const auto n = util::parse_int(value);
    if (!n || !row.range.accepts(static_cast<float>(*n))) return refused();
    return applied(cfg, row, *n, std::to_string(*n));
}

CommandOutcome apply_runtime_entry(config::Settings& cfg, const SettingRow& row, const char* value) {
    const auto n       = util::parse_int(value);
    const auto runtime = n ? runtime_from_ui(*n) : std::nullopt;
    if (!runtime) return refused();
    return applied(cfg, row, *runtime, std::string(config::to_string(*runtime)));
}

} // namespace

bool is_mod_command(std::string_view cmd) noexcept {
    const auto start = cmd.find_first_not_of(' ');
    return start != std::string_view::npos && cmd.substr(start).starts_with(kCommandPrefix);
}

CommandOutcome apply_command(config::Settings& cfg, std::string_view name, const char* value) {
    // Carries no setting value: any value after it is ignored.
    if (name == kCalibrateHeightCommand) return {Kind::Applied, std::nullopt, CommandEffect::CalibrateHeight};
    const SettingRow* row = config::find_command(name);
    if (!row) return {Kind::Unknown};
    switch (row->rule) {
        case CommandRule::Switch: return apply_switch(cfg, *row, value);
        case CommandRule::Choice: return apply_choice(cfg, *row, value);
        case CommandRule::PixelScale: return apply_pixel_scale(cfg, *row, value);
        case CommandRule::Segments: return apply_segments(cfg, *row, value);
        case CommandRule::RuntimeEntry: return apply_runtime_entry(cfg, *row, value);
        case CommandRule::Number: return apply_number(cfg, *row, value);
        case CommandRule::Integer: return apply_integer(cfg, *row, value);
        case CommandRule::None: break;
    }
    return {Kind::Unknown};
}

void Commands::run(char* cmd) noexcept {
    try {
        // First token = command name (strtok_s mutates cmd in place), then
        // the value token; text after it is ignored. Mirrors the strtok-pair
        // pattern throughout FUN_100202e0.
        char* rest       = nullptr;
        const char* name = strtok_s(cmd, kTokenSeparators, &rest);
        if (!name) return;
        const char* val = strtok_s(nullptr, kTokenSeparators, &rest);

        // Before the command changes it: a hand-typed command can come
        // before any settings push has seeded the record, and would
        // otherwise seed it with its own new value and rebuild nothing.
        display_reset_.seed(store_.current());
        const CommandOutcome outcome = apply_command(store_.current(), name, val);
        if (outcome.kind == Kind::Unknown) {
            diag::ports::logf(log_, "[script_bridge] unknown command {} ignored", name);
            return;
        }
        if (outcome.kind == Kind::Refused) {
            diag::ports::logf(log_, "[script_bridge] {} {}: value not accepted, nothing changed", name,
                              val ? val : "(none)");
            return;
        }
        if (outcome.change) {
            diag::ports::logf(log_, "[script_bridge] {} changed to {} by command", outcome.change->path,
                              outcome.change->text);
        }
        carry_out(outcome);
    } catch (...) {
        // Nothing may escape into the game's dispatcher.
        log_.line("[script_bridge] command failed: out of memory");
    }
}

void Commands::carry_out(const CommandOutcome& outcome) {
    // Not a setting: nothing to save; the calibration stores the height
    // it takes itself (spec 22 §6.2).
    if (outcome.effect == CommandEffect::CalibrateHeight) {
        log_.line("[script_bridge] standing height calibration requested by command");
        play_space_.request_height_calibration();
        return;
    }
    config::Settings& cfg = store_.current();
    // Takes effect at once: the game reads the threshold through a pointer
    // patched into its head-in-wall check.
    if (outcome.effect == CommandEffect::ApplyBlinders)
        stereo::set_clipping_blinders(cfg.rendering.clipping_blinders);
    // The 2D effect hooks on the render thread read the keys through atomics.
    // A hook is installed only for a key false at attach (spec 24 §8), so a
    // key turned false that was true then applies at the next launch.
    if (outcome.effect == CommandEffect::ApplyPostFx) {
        post_fx_.set_keys({.lens_flares          = cfg.graphics.lens_flares,
                           .chromatic_aberration = cfg.graphics.chromatic_aberration,
                           .film_grain           = cfg.graphics.film_grain});
    }

    config::sanitize(cfg);
    // Pixel scale and mirror changes need the game's targets rebuilt.
    display_reset_.reset_if_changed(cfg);

    // Persist the updated config to disk. Mirrors the FUN_1000d150() tail
    // call in FUN_100202e0 (an ofstream-based save wrapper). The original
    // also calls FUN_10003490() just before, but the result is unused,
    // likely an artefact of the compiler spilling ECX for a thiscall stub.
    // We omit it.
    store_.save_current();

    // After the save: the new runtime is in the file before the game's
    // main loop reads the quit request. The new runtime takes effect on the
    // next launch, so the game quits, as in the original.
    if (outcome.effect == CommandEffect::QuitGame) signals_.request_exit();
}

} // namespace omvr::core::ui
