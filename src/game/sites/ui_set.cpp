// The UI set of the hook installer (FUN_1001d6f0 steps 2, 4, 5).

#include "game/sites/ui_set.h"

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "game/addresses.h"
#include "game/patcher.h"

#include <cstdint>

namespace omvr::game::sites {

namespace {

namespace diag = core::diag;

// Strings AI.exe is pointed at. They must outlive the process's use of them,
// hence static storage. Originally std::string members at +0x00 and +0x18 of
// the singleton built in FUN_10002090.
constexpr char kMotherPakPath[] = "data\\mother.pak";
constexpr char kSkipBootup[]    = "SkipBootup";

} // namespace

bool redirect_ui_archive(bool archive_built) noexcept {
    if (archive_built && write_dword(ai_address(AiAddr::UiPakPathPtr), address_of(kMotherPakPath))) {
        diag::log_line("[hook] UI archive redirected to data\\mother.pak");
        return true;
    }
    diag::log_line("[hook] no mod UI archive: options page unavailable, continuing");
    return false;
}

void skip_intro(bool mod_archive) noexcept {
    if (!mod_archive) {
        diag::log_line("[hook] startup intro: plays; the options archive is not in use");
    } else if (write_dword(ai_address(AiAddr::BootupMoviePush) + 1, address_of(kSkipBootup))) {
        diag::log_line("[hook] startup intro: skipped");
    } else {
        diag::log_line("[hook] startup intro: plays; the operand could not be written");
    }
}

bool hook_script_bridge(ui::ScriptVars& script_vars, ui::Console& console, const UiReplacements& r) noexcept {
    using namespace ui;
    script_vars.bind(reinterpret_cast<PFN_FireStr2>(ai_address(AiAddr::FireStr2)),
                     reinterpret_cast<PFN_FireBool>(ai_address(AiAddr::FireBool)),
                     reinterpret_cast<PFN_FireInt>(ai_address(AiAddr::FireInt)),
                     reinterpret_cast<PFN_FireFloat>(ai_address(AiAddr::FireFloat)));
    console.bind(reinterpret_cast<PFN_CmdHandler>(ai_address(AiAddr::OriginalCmdHandler)),
                 reinterpret_cast<PFN_MenuMethod>(ai_address(AiAddr::MenuOpen)),
                 ai_address(AiAddr::ExitRequest),
                 ai_address(AiAddr::DisplayResetFlag));
    diag::logf("[hook] script callbacks: fire_bool={:08X} fire_int={:08X} fire_float={:08X}",
               reinterpret_cast<std::uintptr_t>(script_vars.fire_bool()),
               reinterpret_cast<std::uintptr_t>(script_vars.fire_int()),
               reinterpret_cast<std::uintptr_t>(script_vars.fire_float()));

    // All console commands (the UI's ExternalInterface.call("mvr_...")) now
    // pass through handle_mvr_command first.
    if (!write_fn(ai_address(AiAddr::CmdDispatchSlot), r.handle_mvr_command)) return false;
    diag::logf("[hook] cmd dispatch slot {:08X} -> handle_mvr_command",
               reinterpret_cast<std::uintptr_t>(ai_address(AiAddr::CmdDispatchSlot)));

    // The Fire* methods need the game's menu object, so settings are pushed
    // from a hook on it rather than from DLL code with no object at hand.
    return write_fn(ai_address(AiAddr::MenuVtable), r.on_menu_open);
}

bool hook_pause_settings_push(const void* replacement, void*& original) noexcept {
    std::uint8_t* slot = ai_address(AiAddr::PauseMenuVtable);
    original = *reinterpret_cast<void**>(slot);
    return write_fn(slot, replacement);
}

} // namespace omvr::game::sites
