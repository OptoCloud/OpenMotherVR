// OpenMotherVR-Provenance: clean
#pragma once

#include <cstdint>
#include <string_view>

namespace omvr::core::render {

// The dither fix reads the target size from cb2[39] = (W, H, 1/W, 1/H), the
// register the game fills with the render target's resolution (08 §1.3):
// constant buffer 2, register 39.
inline constexpr std::uint32_t kResolutionBuffer = 2;
inline constexpr std::uint32_t kSourceResolution = 39;

// The pixel-shader statistics are logged on the first fixed shader and then
// every this many (11).
inline constexpr int kShaderStatsLogPeriod = 200;

// A shader the driver refuses after the fix is logged this many times.
inline constexpr int kShaderRejectLogLimit = 3;

// ---- Engine settings for VR (24 §6) -----------------------------------------

// The Setting names of the game's DATA\ENGINE_SETTINGS.XML the additions go
// into (24 §1.9), as the game's file spells them.
inline constexpr std::string_view kShadowMapSetting   = "ShadowMapResolution";
inline constexpr std::string_view kReflectionSetting  = "Planar Reflections";
inline constexpr std::string_view kTextureHeapSetting = "Streaming Texture Heap Size";
inline constexpr std::string_view kModelHeapSetting   = "Streaming Model Heap Size";

// Graphics.ShadowMap4096: the shadow map resolution the engine accepts at
// most (24 §1.9), named as the stock options are, by their size.
inline constexpr std::string_view kShadowMap4096Name = "4096";
inline constexpr int kShadowMap4096Value             = 4096;

// Graphics.SharpReflections: a planar reflection quality that is neither
// off (0) nor a mip mode (2, 3), so one mip and no blur (24 §6.2, 07 §2.2).
// Name and value are a project decision (24 §6.2: tester request).
inline constexpr std::string_view kSharpReflectionsName = "Alternative";
inline constexpr int kSharpReflectionsValue             = 6;

// The heap options' names: their size. The game has no menu control for
// these settings, so the name is only seen in the game's own files.
inline constexpr std::string_view kHeapOptionName = "{} MB";

// The copy the game reads instead of its own file, in the game directory
// (13 §2); rebuilt every launch from the player's file.
inline constexpr const wchar_t* kEngineSettingsCopyName = L"openmothervr_engine_settings.xml";

} // namespace omvr::core::render
