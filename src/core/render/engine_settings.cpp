// OpenMotherVR-Provenance: clean
// The engine settings override (24 §6). The scanner below checks that the
// player's file is well-formed XML and finds the Setting elements and their
// direct Quality children; the edit inserts text and keeps every other byte.

#include "core/render/engine_settings.h"

#include "core/diag/log_format.h"
#include "core/render/tuning.h"
#include "util/text/ascii.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <format>
#include <optional>

namespace omvr::core::render {

namespace {

using diag::path_text;
using diag::ports::logf;

constexpr std::string_view kSettingElement = "Setting";
constexpr std::string_view kQualityElement = "Quality";
constexpr std::string_view kNameAttribute  = "name";
constexpr std::string_view kIntAttribute   = "int";
constexpr std::string_view kPrecedence     = "precedence";
constexpr std::string_view kUtf8Bom        = "\xEF\xBB\xBF";

constexpr std::string_view kOverrideOnLog = "[render] engine settings: override on, {} addition(s) for {}";
constexpr std::string_view kReadFailedLog =
    "[render] engine settings: cannot read {} (err=0x{:08X}); nothing added, the game reads its own file";
constexpr std::string_view kParseFailedLog =
    "[render] engine settings: {} is not well-formed XML (at byte {}); nothing added, the game reads its "
    "own file";
constexpr std::string_view kAddedLog = "[render] engine settings: added \"{}\" (int {}, precedence {}) to {}";
constexpr std::string_view kNoSettingLog =
    "[render] engine settings: skipped \"{}\": the file has no {} options";
constexpr std::string_view kAlreadyLog = "[render] engine settings: skipped \"{}\": {} already offers it";
constexpr std::string_view kNothingLog =
    "[render] engine settings: nothing added; the game reads its own file";
constexpr std::string_view kWriteFailedLog =
    "[render] engine settings: cannot write {} (err=0x{:08X}); the game "
    "reads its own file";
constexpr std::string_view kCopyInUseLog = "[render] engine settings: {} is in use and current; using it";
constexpr std::string_view kCopyReadyLog = "[render] engine settings: the game reads {}";

// ---- The scanner ------------------------------------------------------------

struct Quality {
    std::size_t start = 0; // its '<'
    std::size_t end   = 0; // one past its last byte
    std::string_view name;
    std::optional<int> value;
    std::optional<int> precedence;
};

struct SettingInfo {
    std::string_view name;
    std::vector<Quality> qualities;
};

struct Scan {
    bool ok              = false;
    std::size_t error_at = 0;
    std::vector<SettingInfo> settings;
};

struct Attribute {
    std::string_view name;
    std::string_view value;
};

std::optional<int> int_of(std::string_view s) {
    int v          = 0;
    const auto end = s.data() + s.size();
    const auto r   = std::from_chars(s.data(), end, v);
    if (r.ec != std::errc{} || r.ptr != end) return std::nullopt;
    return v;
}

bool is_space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

bool is_name_start(char c) noexcept {
    const auto u = static_cast<unsigned char>(c);
    return (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || u == '_' || u == ':' || u >= 0x80;
}

bool is_name_char(char c) noexcept {
    const auto u = static_cast<unsigned char>(c);
    return is_name_start(c) || (u >= '0' && u <= '9') || u == '-' || u == '.';
}

// A well-formedness check of the parts of XML 1.0 a settings file uses:
// one root element, nested tags, quoted attributes, comments, processing
// instructions, CDATA, a DOCTYPE before the root, entity references.
class Scanner {
public:
    explicit Scanner(std::string_view xml) : x_(xml) {}

    Scan run() {
        if (x_.starts_with(kUtf8Bom)) pos_ = kUtf8Bom.size();
        while (pos_ < x_.size()) {
            if (!step()) return fail();
        }
        if (!open_.empty() || !root_seen_) return fail();
        scan_.ok = true;
        return std::move(scan_);
    }

private:
    struct Open {
        std::string_view name;
        int setting = -1; // this element is settings[setting]
        int owner   = -1; // a Quality of settings[owner]
        int quality = -1; // its index there
    };

    Scan fail() {
        scan_.ok       = false;
        scan_.error_at = pos_;
        scan_.settings.clear();
        return std::move(scan_);
    }

    bool at(std::string_view s) const noexcept { return x_.substr(pos_).starts_with(s); }

    // Moves past `close`, which must follow.
    bool skip_past(std::string_view close) noexcept {
        const auto found = x_.find(close, pos_);
        if (found == std::string_view::npos) return false;
        pos_ = found + close.size();
        return true;
    }

    void skip_spaces() noexcept {
        while (pos_ < x_.size() && is_space(x_[pos_])) ++pos_;
    }

    std::optional<std::string_view> name() noexcept {
        const std::size_t from = pos_;
        if (pos_ >= x_.size() || !is_name_start(x_[pos_])) return std::nullopt;
        while (pos_ < x_.size() && is_name_char(x_[pos_])) ++pos_;
        return x_.substr(from, pos_ - from);
    }

    bool step() {
        const std::size_t lt = x_.find('<', pos_);
        if (!text(x_.substr(pos_, (lt == std::string_view::npos ? x_.size() : lt) - pos_))) return false;
        if (lt == std::string_view::npos) {
            pos_ = x_.size();
            return true;
        }
        pos_ = lt;
        if (at("<?")) return skip_past("?>");
        if (at("<!--")) return skip_past("-->");
        if (at("<![CDATA[")) return !open_.empty() && skip_past("]]>");
        if (at("<!DOCTYPE")) return !root_seen_ && doctype();
        if (at("</")) return end_tag();
        return start_tag();
    }

    // Character data between tags: only white space outside the root; '&'
    // starts a reference ended by ';'.
    bool text(std::string_view t) noexcept {
        for (std::size_t i = 0; i < t.size(); ++i) {
            const char c = t[i];
            if (c == '\0') return false;
            if (open_.empty()) {
                if (!is_space(c)) return false;
                continue;
            }
            if (c == '&') {
                const auto semi = t.find(';', i);
                if (semi == std::string_view::npos || semi == i + 1) return false;
                for (std::size_t k = i + 1; k < semi; ++k) {
                    if (!is_name_char(t[k]) && t[k] != '#') return false;
                }
                i = semi;
            }
        }
        return true;
    }

    bool doctype() noexcept {
        int brackets = 0;
        for (; pos_ < x_.size(); ++pos_) {
            const char c = x_[pos_];
            if (c == '[') ++brackets;
            if (c == ']') --brackets;
            if (c == '>' && brackets == 0) {
                ++pos_;
                return true;
            }
        }
        return false;
    }

    bool end_tag() {
        pos_ += 2;
        const auto n = name();
        if (!n || open_.empty() || open_.back().name != *n) return false;
        skip_spaces();
        if (pos_ >= x_.size() || x_[pos_] != '>') return false;
        ++pos_;
        const Open closed = open_.back();
        open_.pop_back();
        if (closed.owner >= 0) {
            scan_.settings[static_cast<std::size_t>(closed.owner)]
                .qualities[static_cast<std::size_t>(closed.quality)]
                .end = pos_;
        }
        return true;
    }

    bool attributes(std::vector<Attribute>& out, bool& empty) {
        for (;;) {
            const std::size_t before = pos_;
            skip_spaces();
            if (pos_ >= x_.size()) return false;
            if (at("/>")) {
                pos_ += 2;
                empty = true;
                return true;
            }
            if (x_[pos_] == '>') {
                ++pos_;
                empty = false;
                return true;
            }
            // Attributes are separated by white space.
            if (pos_ == before) return false;
            const auto n = name();
            if (!n) return false;
            skip_spaces();
            if (pos_ >= x_.size() || x_[pos_] != '=') return false;
            ++pos_;
            skip_spaces();
            if (pos_ >= x_.size() || (x_[pos_] != '"' && x_[pos_] != '\'')) return false;
            const char quote  = x_[pos_++];
            const auto closer = x_.find(quote, pos_);
            if (closer == std::string_view::npos) return false;
            const auto value = x_.substr(pos_, closer - pos_);
            if (value.find('<') != std::string_view::npos) return false;
            if (std::ranges::any_of(out, [&](const Attribute& a) { return a.name == *n; })) return false;
            out.push_back({*n, value});
            pos_ = closer + 1;
        }
    }

    static std::string_view attribute(const std::vector<Attribute>& attrs, std::string_view key) noexcept {
        const auto it = std::ranges::find_if(attrs, [&](const Attribute& a) { return a.name == key; });
        return it == attrs.end() ? std::string_view{} : it->value;
    }

    bool start_tag() {
        const std::size_t start = pos_;
        ++pos_;
        const auto n = name();
        if (!n) return false;
        if (open_.empty() && root_seen_) return false; // a second root
        std::vector<Attribute> attrs;
        bool empty = false;
        if (!attributes(attrs, empty)) return false;
        if (open_.empty()) root_seen_ = true;

        Open element{*n};
        const int parent_setting = open_.empty() ? -1 : open_.back().setting;
        if (*n == kQualityElement && parent_setting >= 0) {
            auto& qualities = scan_.settings[static_cast<std::size_t>(parent_setting)].qualities;
            qualities.push_back({start, empty ? pos_ : 0, attribute(attrs, kNameAttribute),
                                 int_of(attribute(attrs, kIntAttribute)),
                                 int_of(attribute(attrs, kPrecedence))});
            element.owner   = parent_setting;
            element.quality = static_cast<int>(qualities.size()) - 1;
        } else if (*n == kSettingElement) {
            scan_.settings.push_back({attribute(attrs, kNameAttribute), {}});
            element.setting = static_cast<int>(scan_.settings.size()) - 1;
        }
        if (!empty) open_.push_back(element);
        return true;
    }

    std::string_view x_;
    std::size_t pos_ = 0;
    bool root_seen_  = false;
    std::vector<Open> open_;
    Scan scan_;
};

// ---- The edit ---------------------------------------------------------------

struct Insertion {
    std::size_t at;
    std::string text;
};

std::string escaped(std::string_view s) {
    std::string out;
    for (const char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

// The white space before `start` on its line, if only white space is there.
std::string_view indent_before(std::string_view xml, std::size_t start) noexcept {
    std::size_t from = start;
    while (from > 0 && (xml[from - 1] == ' ' || xml[from - 1] == '\t')) --from;
    if (from > 0 && xml[from - 1] != '\n') return {};
    return xml.substr(from, start - from);
}

bool already_offered(const SettingInfo& s, const EngineOption& o) {
    if (std::ranges::any_of(s.qualities, [&](const Quality& q) { return q.name == o.name; })) return true;
    if (o.rank == OptionRank::Lowest)
        return std::ranges::any_of(s.qualities, [&](const Quality& q) { return q.value == o.value; });
    // The default already has this value: the option would change nothing.
    const auto top =
        std::ranges::max_element(s.qualities, {}, [](const Quality& q) { return q.precedence.value_or(0); });
    return top->value == o.value;
}

int precedence_for(const SettingInfo& s, OptionRank rank) {
    std::optional<int> lo;
    std::optional<int> hi;
    for (const Quality& q : s.qualities) {
        if (!q.precedence) continue;
        lo = std::min(lo.value_or(*q.precedence), *q.precedence);
        hi = std::max(hi.value_or(*q.precedence), *q.precedence);
    }
    if (rank == OptionRank::Lowest) return std::max(lo.value_or(0) - 1, 0);
    return hi.value_or(0) + 1;
}

} // namespace

int EngineSettingsEdit::added() const noexcept {
    return static_cast<int>(
        std::ranges::count_if(results, [](const OptionResult& r) { return r.outcome == AddOutcome::Added; }));
}

std::vector<EngineOption> engine_options(const config::GraphicsSettings& g) {
    std::vector<EngineOption> out;
    if (g.shadow_map_4096) {
        out.push_back(
            {kShadowMapSetting, std::string(kShadowMap4096Name), kShadowMap4096Value, OptionRank::Lowest});
    }
    if (g.sharp_reflections) {
        out.push_back({kReflectionSetting, std::string(kSharpReflectionsName), kSharpReflectionsValue,
                       OptionRank::Lowest});
    }
    const auto heap = [&](std::string_view setting, int mb) {
        if (mb > 0) out.push_back({setting, std::format(kHeapOptionName, mb), mb, OptionRank::Highest});
    };
    heap(kTextureHeapSetting, g.streaming_texture_heap_mb);
    heap(kModelHeapSetting, g.streaming_model_heap_mb);
    return out;
}

EngineSettingsEdit add_engine_options(std::string_view xml, std::span<const EngineOption> options) {
    EngineSettingsEdit edit;
    Scan scan = Scanner{xml}.run();
    if (!scan.ok) {
        edit.error_at = scan.error_at;
        return edit;
    }
    edit.well_formed           = true;
    const std::string_view eol = xml.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";

    std::vector<Insertion> insertions;
    for (const EngineOption& o : options) {
        // The first Setting of that name with options; a later duplicate is
        // left as it is.
        auto it = std::ranges::find_if(
            scan.settings, [&](const SettingInfo& s) { return s.name == o.setting && !s.qualities.empty(); });
        if (it == scan.settings.end()) {
            edit.results.push_back({AddOutcome::NoSetting});
            continue;
        }
        if (already_offered(*it, o)) {
            edit.results.push_back({AddOutcome::AlreadyOffered});
            continue;
        }
        const int precedence = precedence_for(*it, o.rank);
        const Quality last   = it->qualities.back();
        insertions.push_back(
            {last.end, std::format(R"({}{}<Quality name="{}" int="{}" precedence="{}"/>)", eol,
                                   indent_before(xml, last.start), escaped(o.name), o.value, precedence)});
        edit.results.push_back({AddOutcome::Added, precedence});
        // A second option for the same setting ranks against this one too.
        it->qualities.push_back({last.start, last.end, o.name, o.value, precedence});
    }
    if (insertions.empty()) return edit;

    // Stable: options for the same setting keep their order.
    std::ranges::stable_sort(insertions, {}, &Insertion::at);
    std::size_t from = 0;
    for (const Insertion& ins : insertions) {
        edit.text.append(xml.substr(from, ins.at - from));
        edit.text.append(ins.text);
        from = ins.at;
    }
    edit.text.append(xml.substr(from));
    return edit;
}

bool names_engine_settings(std::string_view opened, std::string_view game_path) noexcept {
    if (game_path.empty() || opened.size() < game_path.size()) return false;
    const auto fold        = [](char c) noexcept { return c == '/' ? '\\' : util::ascii_lower(c); };
    const std::size_t tail = opened.size() - game_path.size();
    if (!util::equals_folded(opened.substr(tail), game_path, fold)) return false;
    return tail == 0 || fold(opened[tail - 1]) == '\\';
}

bool build_engine_settings_copy(config::ports::FileStore& files, diag::ports::Log& log,
                                const std::filesystem::path& player_file, const std::filesystem::path& copy,
                                std::span<const EngineOption> options) noexcept {
    try {
        logf(log, kOverrideOnLog, options.size(), path_text(player_file));
        const auto text = files.read(player_file);
        if (!text) {
            logf(log, kReadFailedLog, path_text(player_file), static_cast<std::uint32_t>(text.error().code));
            return false;
        }
        const EngineSettingsEdit edit = add_engine_options(*text, options);
        if (!edit.well_formed) {
            logf(log, kParseFailedLog, path_text(player_file), edit.error_at);
            return false;
        }
        for (std::size_t i = 0; i < options.size(); ++i) {
            const EngineOption& o = options[i];
            switch (edit.results[i].outcome) {
                case AddOutcome::Added:
                    logf(log, kAddedLog, o.name, o.value, edit.results[i].precedence, o.setting);
                    break;
                case AddOutcome::NoSetting: logf(log, kNoSettingLog, o.name, o.setting); break;
                case AddOutcome::AlreadyOffered: logf(log, kAlreadyLog, o.name, o.setting); break;
            }
        }
        if (edit.added() == 0) {
            log.line(kNothingLog);
            return false;
        }
        if (const auto written = files.write_atomically(copy, edit.text); !written) {
            // A second game process (a Steam relaunch) may have the copy
            // open; a copy with this content serves as well.
            const auto existing = files.read(copy);
            if (!existing || *existing != edit.text) {
                logf(log, kWriteFailedLog, path_text(copy), static_cast<std::uint32_t>(written.error().code));
                return false;
            }
            logf(log, kCopyInUseLog, path_text(copy));
            return true;
        }
        logf(log, kCopyReadyLog, path_text(copy));
        return true;
    } catch (...) {
        // Out of memory or a format failure: the game reads its own file.
        return false;
    }
}

} // namespace omvr::core::render
