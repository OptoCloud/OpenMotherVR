// AI.exe version detection.
//
// From: inline code in FUN_10002250 @ 0x10002250 (DllMain) around
//       offsets +0x70..+0x150. We read the headers directly from the
//       already-loaded module: same information, no file I/O.

#include "game/build.h"

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "platform/win32/module.h"
#include "platform/win32/patching/pe_image.h"

namespace omvr::game {

namespace diag = core::diag;
using platform::win32::loaded_image_timestamp;
using AIVersion = GameBuild;

std::optional<GameBuild> identify_ai_exe() {
    void* hmod = platform::win32::module_handle(kGameModuleName);
    if (!hmod) {
        // Some launches load the game as the main exe with a different name
        // (e.g. via Steam re-launch shims). Fall back to the process's own
        // main module.
        hmod = platform::win32::module_handle(nullptr);
    }

    auto ts = loaded_image_timestamp(hmod);
    if (!ts) return std::nullopt;

    switch (*ts) {
        case static_cast<std::uint32_t>(AIVersion::Original_2015): return AIVersion::Original_2015;
        case static_cast<std::uint32_t>(AIVersion::Patched_2020):  return AIVersion::Patched_2020;
        default: return std::nullopt;
    }
}

// Diagnostic helper: returns the actual timestamp we read, regardless of
// whether it matched a known version. Used by the dialog message so the
// user can tell us *what* was found (the original's "Exe check failed! [0x%X]"
// format string).
std::uint32_t ai_exe_timestamp_raw() noexcept {
    void* hmod = platform::win32::module_handle(kGameModuleName);
    if (!hmod) hmod = platform::win32::module_handle(nullptr);
    return loaded_image_timestamp(hmod).value_or(0);
}

std::optional<GameBuild> select_address_table(std::uint8_t* ai_base, AddressMap& addresses) noexcept {
    const auto version = identify_ai_exe();
    if (!version) {
        diag::logf("[hook] unknown AI.exe build (timestamp=0x{:08X}): no patches, game runs as shipped",
                   ai_exe_timestamp_raw());
        return std::nullopt;
    }
    addresses.init(ai_base, *version);
    diag::logf("[hook] AI.exe build {} recognised; image base {:08X}",
               *version == AIVersion::Patched_2020 ? "2020" : "2015", reinterpret_cast<std::uintptr_t>(ai_base));
    return version;
}

} // namespace omvr::game
