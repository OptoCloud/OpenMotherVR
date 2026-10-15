// OpenMotherVR-Provenance: clean
#include "game/detour.h"

#include "platform/win32/patching/inline_hook.h"

#include <cstring>
#include <string_view>

namespace omvr::game {

namespace {

constexpr std::string_view kUnexpectedCodeLog = "[hook] {}: unexpected code at {}, not detoured";

} // namespace

bool detour(std::uint8_t* target, const void* replacement, std::span<const std::uint8_t> expected,
            void** trampoline, const char* name, core::diag::ports::Log& log) noexcept {
    if (!target || !trampoline || std::memcmp(target, expected.data(), expected.size()) != 0) {
        core::diag::ports::logf(log, kUnexpectedCodeLog, name, static_cast<void*>(target));
        return false;
    }
    return platform::win32::install_inline_hook(target, replacement, trampoline, name, log);
}

} // namespace omvr::game
