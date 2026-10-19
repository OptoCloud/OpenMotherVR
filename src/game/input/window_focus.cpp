// OpenMotherVR-Provenance: clean
#include "game/input/window_focus.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/x86.h"

#include <array>
#include <string_view>

namespace omvr::game::input {

namespace {

constexpr std::string_view kNoFocusLog =
    "[hook] window focus: this build has no address for the game's focus flag; background input is not "
    "dropped";
constexpr std::string_view kFocusCodeLog =
    "[hook] window focus: the flag's clear (RVA {:08X}) is not the expected code; background input is not "
    "dropped";
constexpr std::string_view kFocusFoundLog = "[hook] window focus: flag at RVA {:08X}";

// `mov byte [flag], 0`; the address is filled in from the running image.
constexpr std::array<std::uint8_t, x86::kMovAbsImm8Size> kClearFlag = {
    x86::kMovRm8Imm8, x86::kModAbsolute, 0, 0, 0, 0, 0x00};

} // namespace

std::uint8_t* find_window_focus(const GameImage& g, core::diag::ports::Log& log) noexcept {
    const ReconRvas& r = kReconRvas[g.version];
    if (r.window_focus == 0 || r.window_focus_clear == 0) {
        log.line(kNoFocusLog);
        return nullptr;
    }
    std::uint8_t* focus = g.at_rva(r.window_focus);
    if (!x86::matches(g.at_rva(r.window_focus_clear),
                      x86::with_imm32<x86::kMovAbsDispOffset>(kClearFlag, address_of(focus)))) {
        core::diag::ports::logf(log, kFocusCodeLog, r.window_focus_clear);
        return nullptr;
    }
    core::diag::ports::logf(log, kFocusFoundLog, r.window_focus);
    return focus;
}

void clear_window_focus(std::uint8_t* focus) noexcept {
    if (focus != nullptr) *focus = 0;
}

} // namespace omvr::game::input
