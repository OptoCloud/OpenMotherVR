// The UI set of the hook installer (FUN_1001d6f0 steps 2, 4, 5).

#include "game/sites/ui_set.h"

#include "game/code_address.h"

#include <cstdint>
#include <string_view>

namespace omvr::game::sites {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kArchiveRedirectedLog = "[hook] UI archive redirected to data\\mother.pak";
constexpr std::string_view kNoModArchiveLog =
    "[hook] no mod UI archive: options page unavailable, continuing";
constexpr std::string_view kIntroPlaysStockLog =
    "[hook] startup intro: plays; the options archive is not in use";
constexpr std::string_view kIntroSkippedLog = "[hook] startup intro: skipped";
constexpr std::string_view kIntroPlaysFailedLog =
    "[hook] startup intro: plays; the operand could not be written";
constexpr std::string_view kScriptCallbacksLog =
    "[hook] script callbacks: fire_bool={:08X} fire_int={:08X} fire_float={:08X}";
constexpr std::string_view kCmdDispatchLog = "[hook] cmd dispatch slot {:08X} -> handle_mvr_command";

// Strings AI.exe is pointed at. They must outlive the process's use of them,
// hence static storage. Originally std::string members at +0x00 and +0x18 of
// the singleton built in FUN_10002090.
constexpr char kMotherPakPath[] = "data\\mother.pak";
constexpr char kSkipBootup[]    = "SkipBootup";

} // namespace

bool redirect_ui_archive(const GameImage& g, Patcher& p, bool archive_built) noexcept {
    if (archive_built && p.write_pointer(g.at(AiAddr::UiPakPathPtr), kMotherPakPath)) {
        p.log().line(kArchiveRedirectedLog);
        return true;
    }
    p.log().line(kNoModArchiveLog);
    return false;
}

void skip_intro(const GameImage& g, Patcher& p, bool mod_archive) noexcept {
    if (!mod_archive) {
        p.log().line(kIntroPlaysStockLog);
    } else if (p.write_pointer(g.at(AiAddr::BootupMoviePush) + x86::kImm32Offset, kSkipBootup)) {
        p.log().line(kIntroSkippedLog);
    } else {
        p.log().line(kIntroPlaysFailedLog);
    }
}

bool hook_script_bridge(const GameImage& g, Patcher& p, ui::ScriptVars& script_vars, ui::Console& console,
                        const UiReplacements& r) noexcept {
    script_vars.bind(CodeAddress{g.at(AiAddr::FireStr2)}, CodeAddress{g.at(AiAddr::FireBool)},
                     CodeAddress{g.at(AiAddr::FireInt)}, CodeAddress{g.at(AiAddr::FireFloat)});
    console.bind(CodeAddress{g.at(AiAddr::OriginalCmdHandler)}, CodeAddress{g.at(AiAddr::MenuOpen)},
                 g.at(AiAddr::ExitRequest), g.at(AiAddr::DisplayResetFlag));
    ports::logf(p.log(), kScriptCallbacksLog, address_of(g.at(AiAddr::FireBool)),
                address_of(g.at(AiAddr::FireInt)), address_of(g.at(AiAddr::FireFloat)));

    // All console commands (the UI's ExternalInterface.call("mvr_...")) now
    // pass through handle_mvr_command first.
    if (!p.write_pointer(g.at(AiAddr::CmdDispatchSlot), r.handle_mvr_command)) return false;
    ports::logf(p.log(), kCmdDispatchLog, address_of(g.at(AiAddr::CmdDispatchSlot)));

    // The Fire* methods need the game's menu object, so settings are pushed
    // from a hook on it rather than from DLL code with no object at hand.
    return p.write_pointer(g.at(AiAddr::MenuVtable) + slot_offset(kScreenOpenSlot), r.on_menu_open)
        .has_value();
}

bool hook_pause_settings_push(const GameImage& g, Patcher& p, const void* replacement,
                              void*& stock) noexcept {
    std::uint8_t* slot = g.at(AiAddr::PauseMenuVtable) + slot_offset(kScreenOpenSlot);
    stock              = pointer_at(slot);
    return p.write_pointer(slot, replacement).has_value();
}

} // namespace omvr::game::sites
