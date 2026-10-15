// AI.exe version detection.
//
// From: inline code in FUN_10002250 @ 0x10002250 (DllMain) around
//       offsets +0x70..+0x150. We read the headers directly from the
//       already-loaded module: same information, no file I/O.

#include "game/build.h"

#include "game/code_address.h"
#include "platform/win32/module.h"
#include "platform/win32/patching/pe_image.h"

#include <algorithm>
#include <utility>

namespace omvr::game {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kUnknownBuildLog =
    "[hook] unknown AI.exe build (timestamp=0x{:08X}): no patches, game runs as shipped";
constexpr std::string_view kKnownBuildLog = "[hook] AI.exe build {} recognised; image base {:08X}";

// The game's module. Some launches load the game as the main exe with a
// different name (e.g. via Steam re-launch shims): the process's own main
// module then.
void* game_module() noexcept {
    if (void* module = platform::win32::module_handle(kGameModuleName)) return module;
    return platform::win32::module_handle(nullptr);
}

} // namespace

std::optional<GameBuild> identify_ai_exe() noexcept {
    const auto stamp = platform::win32::loaded_image_timestamp(game_module());
    if (!stamp) return std::nullopt;
    const auto known =
        std::ranges::find(kKnownBuilds, *stamp, [](GameBuild b) { return std::to_underlying(b); });
    if (known == kKnownBuilds.end()) return std::nullopt;
    return *known;
}

// The actual timestamp we read, whether or not it matched a known version,
// so the "Exe check failed! [0x%X]" message can say what was found.
std::uint32_t ai_exe_timestamp_raw() noexcept {
    return platform::win32::loaded_image_timestamp(game_module()).value_or(0);
}

std::string_view build_name(GameBuild build) noexcept {
    return build == GameBuild::Patched2020 ? "2020" : "2015";
}

std::optional<GameBuild> select_address_table(std::uint8_t* ai_base, AddressMap& addresses,
                                              ports::Log& log) noexcept {
    const auto version = identify_ai_exe();
    if (!version) {
        ports::logf(log, kUnknownBuildLog, ai_exe_timestamp_raw());
        return std::nullopt;
    }
    addresses.init(ai_base, *version);
    ports::logf(log, kKnownBuildLog, build_name(*version), address_of(ai_base));
    return version;
}

} // namespace omvr::game
