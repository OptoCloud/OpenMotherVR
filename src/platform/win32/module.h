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

} // namespace omvr::platform::win32
