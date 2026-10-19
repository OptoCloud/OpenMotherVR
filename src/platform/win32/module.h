// OpenMotherVR-Provenance: clean
#pragma once

#include <filesystem>
#include <string>

namespace omvr::platform::win32 {

// The directory holding this DLL (the game's install directory); empty if it
// cannot be determined. Works before anything else is set up.
std::filesystem::path module_directory() noexcept;

// The release from the module's own version resource (FILEVERSION, which
// CMake fills from MOTHERVR_VERSION), as "major.minor.patch", or "unknown".
// `module` is the DLL's HINSTANCE.
std::string version_string(void* module);

// The handle of a module loaded in this process by its file name (null for
// the main executable), or null when it is not loaded.
void* module_handle(const char* name) noexcept;

// True when `address` lies in the main executable's image (AI.exe), for
// telling the game's own calls from those of other modules in the process.
bool in_main_executable(const void* address) noexcept;

// GetProcAddress: the export `name` of `module` (an HMODULE), or null.
void* proc_address(void* module, const char* name) noexcept;

// proc_address as a typed function pointer, or nullptr. The one cast from
// an export's address to its type (STYLEGUIDE 4); it goes through void* so
// it does not trip C4191 (FARPROC to an unrelated type).
template <class Fn>
Fn proc(void* module, const char* name) noexcept {
    return reinterpret_cast<Fn>(proc_address(module, name));
}

} // namespace omvr::platform::win32
