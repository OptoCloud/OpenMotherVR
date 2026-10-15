// OpenMotherVR-Provenance: clean
#pragma once

#include <string_view>

namespace omvr::platform::win32 {

// The import address table slot through which the loaded image `module`
// calls `function` of `dll` (both case-insensitive for the DLL, exact for
// the function, imported by name), or null when it imports no such
// function. Swapping the slot (replace_pointer) redirects that image's
// calls only; other modules keep calling the real function.
void** import_slot(void* module, std::string_view dll, std::string_view function) noexcept;

} // namespace omvr::platform::win32
