#pragma once

#include <cstdint>
#include <optional>

namespace mothervr::integration {

// Versions of AlienIsolation.exe (named "AI.exe" on disk) the mod knows
// how to patch. The value is the PE COFF IMAGE_FILE_HEADER.TimeDateStamp.
enum class AIVersion : std::uint32_t {
    Original_2015 = 0x54E3199D,  // launch build
    Patched_2020  = 0x5FD25676,  // Dec 2020 Steam/Epic update
};

// Identify the loaded AI.exe by its PE timestamp.
// Returns std::nullopt if the file isn't an AI build we recognize.
std::optional<AIVersion> identify_ai_exe();

// Read the *this* module's PE timestamp (i.e. the d3d11.dll proxy itself,
// found in the game's install directory) and convert it to a Windows
// FILETIME (100-ns intervals since 1601-01-01 UTC).
//
// The original packed it as: (PE_timestamp * 10'000'000) + FILETIME_UNIX_EPOCH,
// where FILETIME_UNIX_EPOCH = 0x019DB1DED53E8000.
std::int64_t this_module_build_filetime();

// Raw PE timestamp of the loaded AI.exe (zero if not found). Useful for
// logging the value when the build is not one we recognize.
std::uint32_t ai_exe_timestamp_raw() noexcept;

} // namespace mothervr::integration
