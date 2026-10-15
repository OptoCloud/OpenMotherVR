#pragma once

#include "core/diag/ports/log.h"

#include <string_view>

namespace omvr::platform::win32 {

// A message box with `caption` (the caller's product name) for a report to
// the player. `text` is UTF-8; `type` is MB_* flags. The text goes to `log`
// first, named by `kind` if given (a tester searches the log for it; spec 11
// section 2.6). Returns the button pressed, or 0 if the box could not be
// shown (logged). Blocks until the player closes it.
// From: FUN_1001d540 / FUN_1001d5b0.
int message_box(core::diag::ports::Log& log, std::string_view text, unsigned type, const char* kind,
                const wchar_t* caption) noexcept;

} // namespace omvr::platform::win32
