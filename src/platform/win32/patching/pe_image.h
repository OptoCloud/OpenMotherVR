#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace omvr::platform::win32 {

// IMAGE_FILE_HEADER.TimeDateStamp of an already-loaded PE module. nullopt
// if `module` is null or the headers don't look like PE.
std::optional<std::uint32_t> loaded_image_timestamp(const void* module) noexcept;

// SizeOfImage of a loaded 32-bit PE image at `base`.
std::uint32_t image_size_of(const std::uint8_t* base) noexcept;

// IMAGE_FILE_HEADER.Machine of a PE file on disk, 0 if unreadable.
std::uint16_t pe_machine(const std::filesystem::path& file);

// IMAGE_FILE_MACHINE_I386: the machine of a 32-bit x86 image.
inline constexpr std::uint16_t kMachineI386 = 0x014C;

} // namespace omvr::platform::win32
