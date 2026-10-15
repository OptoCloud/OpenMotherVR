#pragma once

#include <cstdint>
#include <string_view>

// Facts about AI.exe other than addresses and layouts.
namespace omvr::game {

// Versions of AlienIsolation.exe (named "AI.exe" on disk) the mod knows
// how to patch. The value is the PE COFF IMAGE_FILE_HEADER.TimeDateStamp.
enum class GameBuild : std::uint32_t {
    Original_2015 = 0x54E3199D,  // launch build
    Patched_2020  = 0x5FD25676,  // Dec 2020 Steam/Epic update
};

// The game's module name, and the PE timestamp's place in it.
inline constexpr const char* kGameModuleName = "AI.exe";

// What the game's own pointers lead to before the UI set replaces them.
inline constexpr std::string_view kStockUiPak = "data\\ui.pak";
inline constexpr std::string_view kShowBootup = "ShowBootup";

// Virtual control codes: map 0 (player) 0x100.., map 1 (UI) 0x200.., map 2
// 0x400... Within a map, digital action k has code base + k and analog
// action j has base + num_digital + j. Real pad codes are 0x00..0x17 (A, B,
// X, Y, ...).
inline constexpr int kTierBase[3] = {0x100, 0x200, 0x400};

// An action in the game's action maps holds at most this many bindings.
inline constexpr int kMaxBindings = 6;

// The x86 opcodes the patches check and write.
inline constexpr std::uint8_t kCall = 0xE8;  // call rel32
inline constexpr std::uint8_t kNop  = 0x90;

} // namespace omvr::game
