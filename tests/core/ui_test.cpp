// core/ui: every mvr_* command rule, the command runner's order (save, then
// quit), the display reset, the settings push, the runtime menu, PAK2 and
// the UI patch.

#include "core/config/settings_store.h"
#include "core/config/tuning.h"
#include "core/input/control_scheme.h"
#include "core/input/glyphs.h"
#include "core/ui/commands.h"
#include "core/ui/display_reset.h"
#include "core/ui/pak2.h"
#include "core/ui/runtime_menu.h"
#include "core/ui/settings_push.h"
#include "core/ui/ui_contract.h"
#include "core/ui/ui_patch.h"
#include "fakes/fakes.h"
#include "util/hash/crc32.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

using namespace omvr::core::ui;
using omvr::core::config::MirrorViewType;
using omvr::core::config::Settings;
using omvr::core::config::VRRuntime;
using omvr::tests::bytes_of;
using omvr::tests::FakeArchiveFiles;
using omvr::tests::FakeGameSignals;
using omvr::tests::FakeRuntimeState;
using omvr::tests::MemoryFileStore;
using omvr::tests::RecordingLog;
using omvr::tests::RecordingScriptVariables;
using omvr::tests::string_of;
using Kind   = CommandOutcome::Kind;
using Chosen = omvr::core::vr::DetectResult::Chosen;

TEST_CASE("is_mod_command: the mvr_ prefix after leading spaces") {
    CHECK(is_mod_command("mvr_debug 1"));
    CHECK(is_mod_command("   mvr_debug 1"));
    CHECK_FALSE(is_mod_command("quit"));
    CHECK_FALSE(is_mod_command(""));
    CHECK_FALSE(is_mod_command("   "));
}

TEST_CASE("switch commands take 0 or 1") {
    Settings s;
    CommandOutcome o = apply_command(s, "mvr_debug", "1");
    CHECK(o.kind == Kind::Applied);
    REQUIRE(o.change.has_value());
    CHECK(o.change->path == "DebugOptions");
    CHECK(o.change->text == "true");
    CHECK(s.debug.debug);

    o = apply_command(s, "mvr_debug", "1");
    CHECK(o.kind == Kind::Applied);
    CHECK_FALSE(o.change.has_value());  // no change

    CHECK(apply_command(s, "mvr_debug", "2").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_debug", nullptr).kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_unknown", "1").kind == Kind::Unknown);

    for (const char* name : {"mvr_mirrorviewsym", "mvr_clipblinding", "mvr_snaprotation", "mvr_inputrelativewalking",
                             "mvr_inputstrafing", "mvr_togglegrips", "mvr_togglecrouch", "mvr_skipintro"}) {
        CHECK(apply_command(s, name, "0").kind == Kind::Applied);
    }
    CHECK_FALSE(s.rendering.symmetric_mirror_view);
    CHECK_FALSE(s.rendering.clipping_blinders);
    CHECK_FALSE(s.input.snap_rotation);
    CHECK_FALSE(s.input.vr_input_relative_walking);
    CHECK_FALSE(s.input.vr_input_strafing);
    CHECK(apply_command(s, "mvr_clipblinding", "1").effect == omvr::core::config::CommandEffect::ApplyBlinders);
}

TEST_CASE("mvr_mirrorview takes the enum's integers") {
    Settings s;
    const CommandOutcome o = apply_command(s, "mvr_mirrorview", "2");
    CHECK(o.kind == Kind::Applied);
    CHECK(o.change->text == "LeftEye");
    CHECK(s.rendering.mirror_view_type == MirrorViewType::LeftEye);
    CHECK(apply_command(s, "mvr_mirrorview", "3").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_mirrorview", "-1").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_mirrorview", "x").kind == Kind::Refused);
}

TEST_CASE("mvr_pixelscale takes a number in range") {
    Settings s;
    const CommandOutcome o = apply_command(s, "mvr_pixelscale", "1.25");
    CHECK(o.kind == Kind::Applied);
    CHECK(o.change->path == "Rendering.PixelScale");
    CHECK(o.change->text == "1.25");
    CHECK(s.rendering.pixel_scale == 1.25f);
    CHECK(apply_command(s, "mvr_pixelscale", "0.5").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_pixelscale", "4").kind == Kind::Applied);
    CHECK(apply_command(s, "mvr_pixelscale", "4.5").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_pixelscale", "0.4").kind == Kind::Refused);
    CHECK(apply_command(s, "mvr_pixelscale", "abc").kind == Kind::Refused);
}

TEST_CASE("mvr_snaprotationsegments rounds down to a step") {
    Settings s;
    const CommandOutcome same = apply_command(s, "mvr_snaprotationsegments", "13");
    CHECK(same.kind == Kind::Applied);
    CHECK_FALSE(same.change.has_value());  // 13 rounds down to 12, the value already set
    CHECK(s.input.snap_rotation_segments == 12);
    CHECK(apply_command(s, "mvr_snaprotationsegments", "100").change->text == "24");
    CHECK(apply_command(s, "mvr_snaprotationsegments", "5").change->text == "4");
    CHECK(apply_command(s, "mvr_snaprotationsegments", "2").change->text == "2");
    CHECK(apply_command(s, "mvr_snaprotationsegments", "1").kind == Kind::Refused);
}

TEST_CASE("mvr_vrruntime maps the options page's entries and quits") {
    Settings s;
    CommandOutcome o = apply_command(s, "mvr_vrruntime", "1");
    CHECK(o.kind == Kind::Applied);
    CHECK(o.effect == omvr::core::config::CommandEffect::QuitGame);
    CHECK(s.vr_runtime == VRRuntime::OpenXR);
    CHECK(o.change->text == "OpenXR");
    CHECK(apply_command(s, "mvr_vrruntime", "2").change->text == "OpenVR");
    CHECK(apply_command(s, "mvr_vrruntime", "0").change->text == "None");
    CHECK(apply_command(s, "mvr_vrruntime", "3").kind == Kind::Refused);
}

TEST_CASE("the runtime menu entries") {
    CHECK(ui_runtime_value(VRRuntime::OpenXR) == RuntimeEntry::OpenXr);
    CHECK(ui_runtime_value(VRRuntime::Oculus) == RuntimeEntry::OpenXr);
    CHECK(ui_runtime_value(VRRuntime::OpenVR) == RuntimeEntry::OpenVr);
    CHECK(ui_runtime_value(VRRuntime::None) == RuntimeEntry::Off);
    CHECK(runtime_from_ui(0) == VRRuntime::None);
    CHECK(runtime_from_ui(1) == VRRuntime::OpenXR);
    CHECK(runtime_from_ui(2) == VRRuntime::OpenVR);
    CHECK_FALSE(runtime_from_ui(5).has_value());

    Settings s;
    FakeRuntimeState state;
    CHECK(running_runtime(s, state) == VRRuntime::OpenVR);
    state.chosen_runtime = Chosen::OpenXR;
    CHECK(running_runtime(s, state) == VRRuntime::OpenXR);
    state.chosen_runtime = Chosen::Oculus;
    CHECK(running_runtime(s, state) == VRRuntime::None);
    state.chosen_runtime = Chosen::OpenVR;
    state.has_failed     = true;
    CHECK(running_runtime(s, state) == VRRuntime::None);
    state.has_failed = false;
    s.vr_runtime     = VRRuntime::None;
    CHECK(running_runtime(s, state) == VRRuntime::None);
    state.forced_runtime = VRRuntime::OpenVR;  // a switch overrides the file
    CHECK(running_runtime(s, state) == VRRuntime::OpenVR);
}

TEST_CASE("display reset: only on a change, only with VR") {
    FakeGameSignals  signals;
    FakeRuntimeState state;
    RecordingLog     log;
    DisplayReset     reset(signals, state, log);
    Settings         s;
    reset.reset_if_changed(s);  // seeds
    reset.reset_if_changed(s);
    CHECK(signals.resets == 0);

    s.rendering.pixel_scale = 1.5f;
    reset.reset_if_changed(s);
    CHECK(signals.resets == 1);
    CHECK(log.lines.back() == "[script_bridge] display reset raised; changed: Rendering.PixelScale");

    s.rendering.mirror_view_type      = MirrorViewType::BothEyes;
    s.rendering.symmetric_mirror_view = false;
    reset.reset_if_changed(s);
    CHECK(log.lines.back() ==
          "[script_bridge] display reset raised; changed: Rendering.MirrorViewType Rendering.SymmetricMirrorView");

    s.vr_runtime            = VRRuntime::None;
    s.rendering.pixel_scale = 2.0f;
    reset.reset_if_changed(s);
    CHECK(signals.resets == 2);  // no VR this run: nothing to rebuild

    signals.can_reset       = false;
    s.vr_runtime            = VRRuntime::OpenVR;
    s.rendering.pixel_scale = 1.0f;
    reset.reset_if_changed(s);
    CHECK(signals.resets == 2);
}

TEST_CASE("the command runner saves, then quits, after mvr_vrruntime") {
    std::vector<std::string> events;
    MemoryFileStore          files;
    files.events = &events;
    RecordingLog                           log;
    omvr::core::config::SettingsStore      store(files, log);
    store.initialize(L"C:\\game");
    FakeGameSignals  signals;
    signals.events = &events;
    FakeRuntimeState state;
    DisplayReset     reset(signals, state, log);
    Commands         commands(store, reset, signals, log);

    char cmd[] = "mvr_vrruntime 1 trailing";
    commands.run(cmd);
    CHECK(store.current().vr_runtime == VRRuntime::OpenXR);
    REQUIRE(events.size() == 2);
    CHECK(events[0] == "write");
    CHECK(events[1] == "exit");
    CHECK(log.contains("[script_bridge] VRRuntime changed to OpenXR by command"));

    char refused[] = "mvr_debug 7";
    commands.run(refused);
    CHECK(log.lines.back() == "[script_bridge] mvr_debug 7: value not accepted, nothing changed");
    char unknown[] = "mvr_what 1";
    commands.run(unknown);
    CHECK(log.lines.back() == "[script_bridge] unknown command mvr_what ignored");
    char none[] = "mvr_debug";
    commands.run(none);
    CHECK(log.lines.back() == "[script_bridge] mvr_debug (none): value not accepted, nothing changed");
    CHECK(files.writes == 1);
}

TEST_CASE("the settings push sets every variable, then the controls help") {
    RecordingScriptVariables vars;
    FakeGameSignals          signals;
    FakeRuntimeState         state;
    RecordingLog             log;
    DisplayReset             reset(signals, state, log);
    omvr::core::input::Glyphs         glyphs;
    omvr::core::input::ControlSchemes schemes(glyphs);
    Settings s;
    s.rendering.pixel_scale = 1.5f;
    SettingsPush push({vars, reset, schemes, s, state, log});

    push.push(ports::UiHandle{});
    CHECK(vars.calls.empty());
    push.push(ports::UiHandle{});
    CHECK(log.lines.size() == 1);  // logged once

    push.push(ports::UiHandle{1});
    const std::vector<std::string> head(vars.calls.begin(), vars.calls.begin() + 14);
    CHECK(head == std::vector<std::string>{
                      "setMotherVROptionsEnabled=true",
                      "setMotherSetVRRuntime=2",
                      "setMotherDebug=0",
                      "setMotherSkipIntro=false",
                      "setMotherMirrorView=1",
                      "setMotherMirrorViewSym=true",
                      "setMotherPixelScale=1.500000",
                      "setMotherClipBlinding=true",
                      "setMotherSnapRotation=true",
                      "setMotherSnapRotationSegments=12",
                      "setMotherInputRelativeWalking=true",
                      "setMotherInputStrafing=true",
                      "setMotherToggleGrips=false",
                      "setMotherToggleCrouch=false",
                  });
    CHECK(vars.calls[14] == "addMotherViewControl=Move|L Touchpad");
    CHECK(vars.calls.size() == 14 + schemes.control_help().size());

    // A row the game refuses stops the list.
    RecordingScriptVariables short_vars;
    short_vars.row_limit = 2;
    SettingsPush stops({short_vars, reset, schemes, s, state, log});
    stops.push(ports::UiHandle{1});
    CHECK(short_vars.rows == 2);
}

TEST_CASE("PAK2 round trip") {
    const std::vector<OutEntry> entries = {{"a", bytes_of("abc")}, {"bb", bytes_of("d")}};
    const std::vector<std::byte> pak = build_pak(16, entries);
    // Header 16, names "a\0bb\0" 5, table 8: entry a at 32, bb at 48.
    CHECK(pak.size() == 49u);
    CHECK(string_of(std::span(pak).first(4)) == "PAK2");

    const auto read = read_pak(pak);
    REQUIRE(read.has_value());
    CHECK(read->size() == 2u);
    CHECK(string_of(read->at("a")) == "abc");
    CHECK(string_of(read->at("bb")) == "d");

    std::vector<std::byte> bad = pak;
    bad[0] = std::byte{'X'};
    CHECK_FALSE(read_pak(bad).has_value());
    CHECK_FALSE(read_pak(std::span(pak).first(20)).has_value());
    std::vector<std::byte> zero_align = pak;
    std::memset(zero_align.data() + 12, 0, 4);
    CHECK_FALSE(read_pak(zero_align).has_value());
}

// A UI patch (MVRUIP01) built byte by byte, as scripts/ui_patch.py writes it.
class PatchWriter {
public:
    PatchWriter(std::uint32_t align, std::uint32_t count) {
        text("MVRUIP01");
        u32(align);
        u32(count);
    }
    void entry(std::string_view name, PatchEntry kind) {
        u16(static_cast<std::uint16_t>(name.size()));
        text(name);
        u8(static_cast<std::uint8_t>(kind));
    }
    void u8(std::uint8_t v) { out.push_back(std::byte{v}); }
    void u16(std::uint16_t v) { raw(&v, sizeof(v)); }
    void u32(std::uint32_t v) { raw(&v, sizeof(v)); }
    void text(std::string_view s) { raw(s.data(), s.size()); }

    std::vector<std::byte> out;

private:
    void raw(const void* p, std::size_t n) {
        const auto* b = static_cast<const std::byte*>(p);
        out.insert(out.end(), b, b + n);
    }
};

std::uint32_t crc_of(std::string_view s) { return omvr::util::crc32(bytes_of(s)); }

TEST_CASE("the UI patch copies, rewrites and adds entries") {
    FakeArchiveFiles files;
    RecordingLog     log;
    const std::filesystem::path stock  = L"DATA\\UI.PAK";
    const std::filesystem::path target = L"DATA\\MOTHER.PAK";
    files.files[stock] = build_pak(4, {{"keep", bytes_of("hello")}, {"edit", bytes_of("abcdef")}});

    PatchWriter p(4, 3);
    p.entry("keep", PatchEntry::Stock);
    p.u32(5);
    p.u32(crc_of("hello"));
    p.entry("edit", PatchEntry::Delta);
    p.u32(6);
    p.u32(crc_of("abcdef"));
    p.u32(5);  // output size
    p.u32(2);  // operations
    p.u8(static_cast<std::uint8_t>(DeltaOp::Copy));
    p.u32(0);
    p.u32(3);
    p.u8(static_cast<std::uint8_t>(DeltaOp::Insert));
    p.u32(2);
    p.text("XY");
    p.entry("new", PatchEntry::New);
    p.u32(3);
    p.text("new");

    REQUIRE(build_ui_archive(files, log, stock, p.out, target));
    const auto built = read_pak(files.files.at(target));
    REQUIRE(built.has_value());
    CHECK(string_of(built->at("keep")) == "hello");
    CHECK(string_of(built->at("edit")) == "abcXY");
    CHECK(string_of(built->at("new")) == "new");
    CHECK(log.contains("ui patch: built"));
}

TEST_CASE("the UI patch refuses another stock version and a malformed patch") {
    FakeArchiveFiles files;
    RecordingLog     log;
    const std::filesystem::path stock  = L"UI.PAK";
    const std::filesystem::path target = L"MOTHER.PAK";
    files.files[stock] = build_pak(4, {{"keep", bytes_of("HELLO")}});

    PatchWriter p(4, 1);
    p.entry("keep", PatchEntry::Stock);
    p.u32(5);
    p.u32(crc_of("hello"));
    CHECK_FALSE(build_ui_archive(files, log, stock, p.out, target));
    CHECK(log.contains("ui patch: keep in UI.PAK is not the version the patch was made for"));
    CHECK_FALSE(files.files.contains(target));

    std::vector<std::byte> bad = p.out;
    bad[0] = std::byte{'X'};
    CHECK_FALSE(build_ui_archive(files, log, stock, bad, target));
    CHECK(log.lines.back() == "ui patch: embedded patch is malformed");

    CHECK_FALSE(build_ui_archive(files, log, L"missing.pak", p.out, target));
    CHECK(log.lines.back() == "ui patch: cannot read missing.pak");

    // A copy that reaches past the stock entry.
    files.files[stock] = build_pak(4, {{"e", bytes_of("abc")}});
    PatchWriter d(4, 1);
    d.entry("e", PatchEntry::Delta);
    d.u32(3);
    d.u32(crc_of("abc"));
    d.u32(4);
    d.u32(1);
    d.u8(static_cast<std::uint8_t>(DeltaOp::Copy));
    d.u32(1);
    d.u32(4);
    CHECK_FALSE(build_ui_archive(files, log, stock, d.out, target));
}

TEST_CASE("the UI patch reports a temporary file it cannot create") {
    FakeArchiveFiles files;
    RecordingLog     log;
    files.files[L"UI.PAK"] = build_pak(4, {{"n", bytes_of("x")}});
    files.fail_create      = true;
    PatchWriter p(4, 1);
    p.entry("n", PatchEntry::New);
    p.u32(1);
    p.text("y");
    CHECK_FALSE(build_ui_archive(files, log, L"UI.PAK", p.out, L"MOTHER.PAK"));
    CHECK(log.contains("ui patch: cannot create MOTHER.PAK.tmp err=0x00000005"));
}

} // namespace
