// OpenMotherVR-Provenance: clean
// The engine settings load (24 §1.9): checked, never written.

#include "game/sites/engine_settings.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/sites/render_fixes.h"
#include "game/x86.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace omvr::game::sites {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kNoAddressWhy = "this build has no address for it";
constexpr std::string_view kCodeLog =
    "[hook] engine settings: off; the code at 0x{:X} is not the expected settings load";
constexpr std::string_view kPathLog  = "[hook] engine settings: off; the path at 0x{:X} is not a file name";
constexpr std::string_view kFoundLog = "[hook] engine settings: the game loads \"{}\" at 0x{:X}";

// After the 5-byte `mov eax, [moffs32]`: `push 1; push eax; lea ecx,
// [esp+0x38]`, then the call.
constexpr std::uint8_t kLoadFlag                = 1;
constexpr std::uint8_t kParserOffset            = 0x38;
constexpr std::array<std::uint8_t, 7> kLoadArgs = {x86::kPushImm8,   kLoadFlag,    x86::kPushEax, x86::kLea,
                                                   x86::kModRmClSib, x86::kSibEsp, kParserOffset};
constexpr std::size_t kMovEaxMoffsSize          = x86::kImm32Offset + x86::kRel32Size;
constexpr std::size_t kCallOffset               = kMovEaxMoffsSize + kLoadArgs.size();

// The game's path is a short relative file name (MAX_PATH bounds it).
constexpr std::size_t kMaxPath = 260;

} // namespace

std::string_view engine_settings_path(const GameImage& g, ports::Log& log) noexcept {
    const EngineSettingsRvas& r = engine_settings_rvas(g.version);
    if (r.load_site == 0) {
        area_off(log, kEngineSettingsArea, kNoAddressWhy);
        return {};
    }
    const std::uint8_t* site = g.at_rva(r.load_site);
    if (site[0] != x86::kMovEaxMoffs ||
        read_u32(site + x86::kImm32Offset) != address_of(g.at_rva(r.path_pointer)) ||
        !x86::matches(site + kMovEaxMoffsSize, kLoadArgs) ||
        !calls(site + kCallOffset, g.at_rva(r.xml_load))) {
        ports::logf(log, kCodeLog, r.load_site);
        return {};
    }
    const std::uint32_t text = read_u32(g.at_rva(r.path_pointer));
    if (!g.contains(text)) {
        ports::logf(log, kPathLog, r.path_pointer);
        return {};
    }
    const char* path = CodeAddress{text};
    std::size_t n    = 0;
    while (n < kMaxPath && g.contains(text + n) && path[n] != '\0') ++n;
    if (n == 0 || n == kMaxPath || !g.contains(text + n)) {
        ports::logf(log, kPathLog, r.path_pointer);
        return {};
    }
    const std::string_view found{path, n};
    ports::logf(log, kFoundLog, found, r.load_site);
    return found;
}

} // namespace omvr::game::sites
