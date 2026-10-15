// core/config: reading mothervr.json in both layouts, sanitizing, writing
// with unknown keys kept, describe, the settings store over a file store,
// and the -vrforce* and -vrscale switches.

#include "core/config/launch_options.h"
#include "core/config/settings_json.h"
#include "core/config/settings_store.h"
#include "core/config/settings_table.h"
#include "core/config/tuning.h"
#include "fakes/fakes.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

using namespace omvr::core::config;
using nlohmann::json;
using omvr::tests::MemoryFileStore;
using omvr::tests::RecordingLog;

TEST_CASE("defaults") {
    const Settings s{};
    CHECK(s.vr_runtime == VRRuntime::OpenVR);
    CHECK(s.rendering.mirror_view_type == MirrorViewType::RightEye);
    CHECK(s.rendering.symmetric_mirror_view);
    CHECK(s.rendering.pixel_scale == 1.0f);
    CHECK(s.rendering.clipping_blinders);
    CHECK(s.rendering.stereo_aa == StereoAA::Auto);
    CHECK(s.input.snap_rotation);
    CHECK(s.input.snap_rotation_segments == 12);
    CHECK(s.input.vr_input_relative_walking);
    CHECK(s.input.vr_input_strafing);
    CHECK_FALSE(s.input.toggle_grips);
    CHECK_FALSE(s.input.toggle_crouch);
    CHECK_FALSE(s.debug.debug);
    CHECK_FALSE(s.debug.skip_startup_intro);
}

TEST_CASE("read_config reads every key of the original layout") {
    const json root  = json::parse(R"({"Settings": {
        "VRRuntime": "OpenXR", "DebugOptions": true, "SkipStartupIntro": true,
        "Rendering": {"MirrorViewType": "LeftEye", "SymmetricMirrorView": false, "PixelScale": 1.5,
                      "ClippingBlinders": false, "StereoAntiAliasing": "On"},
        "Input": {"SnapRotation": false, "SnapRotationSegments": 24, "VRInputRelativeWalking": false,
                  "VRInputStrafing": false, "ToggleGrips": true, "ToggleCrouch": true}}})");
    const Settings s = read_config(root);
    CHECK(s.vr_runtime == VRRuntime::OpenXR);
    CHECK(s.debug.debug);
    CHECK(s.debug.skip_startup_intro);
    CHECK(s.rendering.mirror_view_type == MirrorViewType::LeftEye);
    CHECK_FALSE(s.rendering.symmetric_mirror_view);
    CHECK(s.rendering.pixel_scale == 1.5f);
    CHECK_FALSE(s.rendering.clipping_blinders);
    CHECK(s.rendering.stereo_aa == StereoAA::On);
    CHECK_FALSE(s.input.snap_rotation);
    CHECK(s.input.snap_rotation_segments == 24);
    CHECK_FALSE(s.input.vr_input_relative_walking);
    CHECK_FALSE(s.input.vr_input_strafing);
    CHECK(s.input.toggle_grips);
    CHECK(s.input.toggle_crouch);
}

TEST_CASE("read_config takes the original's integers for enums") {
    const Settings s = read_config(json::parse(R"({"Settings": {"VRRuntime": 3,
        "Rendering": {"MirrorViewType": 0, "StereoAntiAliasing": 2}}})"));
    CHECK(s.vr_runtime == VRRuntime::OpenXR);
    CHECK(s.rendering.mirror_view_type == MirrorViewType::BothEyes);
    CHECK(s.rendering.stereo_aa == StereoAA::Off);

    const Settings out_of_range =
        read_config(json::parse(R"({"Settings": {"VRRuntime": 4, "Rendering": {"MirrorViewType": -1}}})"));
    CHECK(out_of_range.vr_runtime == VRRuntime::OpenVR);
    CHECK(out_of_range.rendering.mirror_view_type == MirrorViewType::RightEye);
}

TEST_CASE("read_config: legacy spellings and layout") {
    CHECK(read_config(json::parse(R"({"Settings": {"VRRuntime": "Disabled"}})")).vr_runtime ==
          VRRuntime::None);
    CHECK(read_config(json::parse(R"({"Settings": {"VRRuntime": "Oculus"}})")).vr_runtime ==
          VRRuntime::Oculus);

    const Settings legacy =
        read_config(json::parse(R"({"DebugOptions": {"SkipStartupIntro": true, "DevDebugOptions": true}})"));
    CHECK(legacy.debug.skip_startup_intro);
    CHECK(legacy.debug.debug);

    // The original's keys win over the legacy object.
    const Settings both = read_config(
        json::parse(R"({"DebugOptions": {"DevDebugOptions": true}, "Settings": {"DebugOptions": false}})"));
    CHECK_FALSE(both.debug.debug);
}

TEST_CASE("read_config: wrong types and missing parts give defaults") {
    const Settings s = read_config(json::parse(R"({"Settings": {"VRRuntime": "SteamVR",
        "Rendering": {"PixelScale": "big"}, "Input": {"SnapRotation": 1, "SnapRotationSegments": "6"}}})"));
    CHECK(s.vr_runtime == VRRuntime::OpenVR);
    CHECK(s.rendering.pixel_scale == 1.0f);
    CHECK(s.input.snap_rotation);
    CHECK(s.input.snap_rotation_segments == 12);

    CHECK(read_config(json::parse("[]")).vr_runtime == VRRuntime::OpenVR);
    CHECK(read_config(json::parse(R"({"Settings": 3})")).input.snap_rotation_segments == 12);
    CHECK(read_config(json::parse(R"({"Settings": {"Rendering": 3}})")).rendering.pixel_scale == 1.0f);
}

TEST_CASE("sanitize: Oculus is OpenXR, pixel scale range, snap segment steps") {
    Settings s;
    s.vr_runtime                   = VRRuntime::Oculus;
    s.rendering.pixel_scale        = 10.0f;
    s.input.snap_rotation_segments = 13;
    sanitize(s);
    CHECK(s.vr_runtime == VRRuntime::OpenXR);
    CHECK(s.rendering.pixel_scale == 1.0f);
    CHECK(s.input.snap_rotation_segments == 12);

    s.rendering.pixel_scale = std::numeric_limits<float>::quiet_NaN();
    sanitize(s);
    CHECK(s.rendering.pixel_scale == 1.0f);
    s.rendering.pixel_scale = kPixelScaleMin;
    sanitize(s);
    CHECK(s.rendering.pixel_scale == kPixelScaleMin);
    s.rendering.pixel_scale = kPixelScaleMax;
    sanitize(s);
    CHECK(s.rendering.pixel_scale == kPixelScaleMax);

    CHECK(snap_segments(100) == 24);
    CHECK(snap_segments(24) == 24);
    CHECK(snap_segments(11) == 6);
    CHECK(snap_segments(5) == 4);
    CHECK(snap_segments(3) == 3);
    CHECK(snap_segments(2) == 2);
    CHECK(snap_segments(1) == 12);
    CHECK(snap_segments(-5) == 12);
}

TEST_CASE("write_config keeps unknown keys, drops the legacy object, never writes Oculus") {
    json root = json::parse(R"({"DebugOptions": {"DevDebugOptions": true}, "Other": 1,
        "Settings": {"Custom": "x", "Rendering": {"Extra": 2}}})");
    Settings s;
    s.vr_runtime                 = VRRuntime::Oculus;
    s.rendering.mirror_view_type = MirrorViewType::BothEyes;
    write_config(root, s);
    CHECK_FALSE(root.contains("DebugOptions"));
    CHECK(root["Other"] == 1);
    CHECK(root["Settings"]["Custom"] == "x");
    CHECK(root["Settings"]["Rendering"]["Extra"] == 2);
    CHECK(root["Settings"]["VRRuntime"] == "OpenXR");
    CHECK(root["Settings"]["Rendering"]["MirrorViewType"] == "BothEyes");
    CHECK(root["Settings"]["Rendering"]["StereoAntiAliasing"] == "Auto");
    CHECK(root["Settings"]["Input"]["SnapRotationSegments"] == 12);
    CHECK(root["Settings"]["DebugOptions"] == false);
    CHECK(root["Settings"]["SkipStartupIntro"] == false);

    // Every row is written, and reading it back gives the same settings.
    Settings back = read_config(root);
    CHECK(back.rendering.mirror_view_type == MirrorViewType::BothEyes);
    CHECK(back.vr_runtime == VRRuntime::OpenXR);

    json not_object = json::array();
    write_config(not_object, Settings{});
    CHECK(not_object.is_object());
}

TEST_CASE("describe names each setting by its key path") {
    const std::string d = describe(Settings{});
    CHECK(d.front() == '{');
    CHECK(d.find(R"("VRRuntime":"OpenVR")") != std::string::npos);
    CHECK(d.find(R"("Rendering.PixelScale":1.0)") != std::string::npos);
    CHECK(d.find(R"("Input.SnapRotationSegments":12)") != std::string::npos);
    CHECK(d.find(R"("DebugOptions":false)") != std::string::npos);
    CHECK(json::parse(d).size() == settings_table().size());
}

TEST_CASE("the settings table: commands and their rules") {
    CHECK(settings_table().size() == 14);
    const SettingRow* row = find_command("mvr_pixelscale");
    REQUIRE(row != nullptr);
    CHECK(row->rule == CommandRule::PixelScale);
    CHECK(row->path == "Rendering.PixelScale");
    CHECK(find_command("mvr_vrruntime")->effect == CommandEffect::QuitGame);
    CHECK(find_command("mvr_clipblinding")->effect == CommandEffect::ApplyBlinders);
    CHECK(find_command("mvr_nothing") == nullptr);
    CHECK(find_command("") == nullptr); // the AA row has no command
}

TEST_CASE("enum names round trip") {
    CHECK(to_string(VRRuntime::None) == "None");
    CHECK(to_string(VRRuntime::Oculus) == "Oculus");
    CHECK(to_string(MirrorViewType::LeftEye) == "LeftEye");
    CHECK(to_string(StereoAA::Off) == "Off");
    CHECK(parse_vr_runtime("OpenVR") == VRRuntime::OpenVR);
    CHECK_FALSE(parse_vr_runtime("openvr").has_value());
    CHECK(parse_mirror_view_type("BothEyes") == MirrorViewType::BothEyes);
    CHECK(parse_stereo_aa("Auto") == StereoAA::Auto);
}

const std::filesystem::path kDir  = L"C:\\game";
const std::filesystem::path kFile = kDir / kSettingsFileName;

TEST_CASE("settings store: first launch, a byte-order mark, an unusable file") {
    MemoryFileStore files;
    RecordingLog log;
    SettingsStore store(files, log);
    CHECK(store.initialize(kDir)); // absent: the caller writes the template
    CHECK(store.load_source() == "defaults, first launch");

    files.files[kFile] =
        "\xEF\xBB\xBF"
        R"({"Settings": {"VRRuntime": "OpenXR"}})";
    SettingsStore bom(files, log);
    CHECK_FALSE(bom.initialize(kDir));
    CHECK(bom.current().vr_runtime == VRRuntime::OpenXR);
    CHECK(bom.load_source() == "file");

    files.files[kFile] = "{ not json";
    SettingsStore broken(files, log);
    CHECK_FALSE(broken.initialize(kDir)); // there, but unusable: kept until a change
    CHECK(broken.load_source() == "defaults, file unusable");
    CHECK(broken.current().vr_runtime == VRRuntime::OpenVR);
}

TEST_CASE("settings store: save keeps unknown keys and writes four-space JSON") {
    MemoryFileStore files;
    RecordingLog log;
    files.files[kFile] = R"({"Mine": true, "Settings": {"VRRuntime": "Oculus"}})";
    SettingsStore store(files, log);
    store.initialize(kDir);
    CHECK(store.current().vr_runtime == VRRuntime::OpenXR); // sanitized
    store.current().rendering.pixel_scale = 2.0f;
    store.save_current();
    CHECK(files.writes == 1);
    const std::string text = files.files[kFile];
    CHECK(text.find("\n    \"Mine\": true") != std::string::npos);
    const json back = json::parse(text);
    CHECK(back["Settings"]["Rendering"]["PixelScale"] == 2.0);
    CHECK(back["Settings"]["VRRuntime"] == "OpenXR");
    CHECK(log.contains("config: settings saved to"));
}

TEST_CASE("-vrforce* switches: exact tokens, any case, the last wins") {
    CHECK_FALSE(forced_runtime_in(L"AI.exe").has_value());
    CHECK(forced_runtime_in(L"AI.exe -vrforceopenvr") == VRRuntime::OpenVR);
    CHECK(forced_runtime_in(L"AI.exe -vrforceopenvr -VRFORCEOCULUS") == VRRuntime::OpenXR);
    CHECK(forced_runtime_in(L"AI.exe -vrforceopenxr -vrforcenone") == VRRuntime::None);
    CHECK(forced_runtime_in(L"AI.exe \"-vrforceopenvr\"") == VRRuntime::OpenVR);
    CHECK_FALSE(forced_runtime_in(L"AI.exe -vrforceopenvrx").has_value());
    CHECK_FALSE(forced_runtime_in(L"\"C:\\Games\\-vrforcenone\\AI.exe\"").has_value());
}

TEST_CASE("-vrscale: the next argument, within range, the last wins") {
    using Args = std::vector<std::wstring>;
    CHECK(world_scale_in(Args{L"AI.exe"}) == kWorldScaleDefault);
    CHECK(world_scale_in(Args{L"AI.exe", L"-vrscale", L"1.2"}) == 1.2f);
    CHECK(world_scale_in(Args{L"AI.exe", L"-VRScale", L"0.5"}) == 0.5f);
    CHECK(world_scale_in(Args{L"AI.exe", L"-vrscale", L"2.0"}) == kWorldScaleDefault);
    CHECK(world_scale_in(Args{L"AI.exe", L"-vrscale", L"abc"}) == kWorldScaleDefault);
    CHECK(world_scale_in(Args{L"AI.exe", L"-vrscale"}) == kWorldScaleDefault);
    CHECK(world_scale_in(Args{L"AI.exe", L"-vrscale", L"1.2", L"-vrscale", L"0.8"}) == 0.8f);
    CHECK(world_scale_in(Args{L"AI.exe", L"-vrscale", L"1.2", L"-vrscale", L"9"}) == kWorldScaleDefault);
    CHECK(world_scale_in(Args{L"-vrscale", L"1.2"}) == kWorldScaleDefault); // args[0] is the program
}

} // namespace
