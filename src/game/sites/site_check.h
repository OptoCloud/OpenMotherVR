// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "game/code_address.h"
#include "game/game_image.h"
#include "game/patcher.h"
#include "game/x86.h"

#include <cstdint>
#include <cstring>
#include <string_view>

// The checks a site installer runs before it writes anything (12 §4): each
// passes or logs which code or data of its area differs, and the area is
// off.
namespace omvr::game::sites {

inline constexpr std::string_view kUnexpectedLog =
    "[hook] {}: off; unexpected code or data at {} (RVA {:08X})";

struct Check {
    const GameImage& g;
    Patcher& p;
    std::string_view area;

    // Logs that `what` at `rva` differs; false.
    bool fail(std::string_view what, std::uint32_t rva) const noexcept {
        core::diag::ports::logf(p.log(), kUnexpectedLog, area, what, rva);
        return false;
    }
    // The bytes (or x86::Pattern) at `rva`.
    bool code(std::uint32_t rva, std::string_view what, const auto& expected) const noexcept {
        return x86::matches(g.at_rva(rva), expected) || fail(what, rva);
    }
    // `call rel32` at `site` reaching the function at `target`.
    bool call(std::uint32_t site, std::uint32_t target, std::string_view what) const noexcept {
        return calls(g.at_rva(site), g.at_rva(target)) || fail(what, site);
    }
    // The method-table entry at `slot` holds the function at `rva`.
    bool entry(std::uint32_t slot, std::uint32_t rva, std::string_view what) const noexcept {
        return holds_address(g.at_rva(slot), g.at_rva(rva)) || fail(what, slot);
    }
    // The NUL-terminated `text` at `rva`.
    bool string(std::uint32_t rva, std::string_view what, std::string_view text) const noexcept {
        const auto* s = g.at_rva(rva);
        return (std::memcmp(s, text.data(), text.size()) == 0 && s[text.size()] == 0) || fail(what, rva);
    }
    // `push imm32` at `rva` naming the string at `string_rva`.
    bool push_of(std::uint32_t rva, std::uint32_t string_rva, std::string_view what) const noexcept {
        return pushes(g.at_rva(rva), g.at_rva(string_rva)) || fail(what, rva);
    }
};

} // namespace omvr::game::sites
