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
                if (!n || *n < 0 || *n > std::to_underlying(config::EnumTraits<T>::kLast)) return refused();
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
    const SettingRow* row = config::find_command(name);
    if (!row) return {Kind::Unknown};
    switch (row->rule) {
        case CommandRule::Switch: return apply_switch(cfg, *row, value);
        case CommandRule::Choice: return apply_choice(cfg, *row, value);
        case CommandRule::PixelScale: return apply_pixel_scale(cfg, *row, value);
        case CommandRule::Segments: return apply_segments(cfg, *row, value);
        case CommandRule::RuntimeEntry: return apply_runtime_entry(cfg, *row, value);
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
    config::Settings& cfg = store_.current();
    // Takes effect at once: the game reads the threshold through a pointer
    // patched into its head-in-wall check.
    if (outcome.effect == config::CommandEffect::ApplyBlinders)
        stereo::set_clipping_blinders(cfg.rendering.clipping_blinders);

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
    if (outcome.effect == config::CommandEffect::QuitGame) signals_.request_exit();
}

} // namespace omvr::core::ui
