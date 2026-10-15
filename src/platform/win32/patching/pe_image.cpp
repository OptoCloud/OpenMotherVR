#include "platform/win32/patching/pe_image.h"

#include <windows.h>

#include <fstream>

namespace omvr::platform::win32 {

static_assert(kMachineI386 == IMAGE_FILE_MACHINE_I386);

std::optional<std::uint32_t> loaded_image_timestamp(const void* module) noexcept {
    if (!module) return std::nullopt;
    auto* base = static_cast<const std::uint8_t*>(module);

    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return std::nullopt;

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return std::nullopt;

    return nt->FileHeader.TimeDateStamp;
}

std::uint32_t image_size_of(const std::uint8_t* base) noexcept {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    return nt->OptionalHeader.SizeOfImage;
}

std::uint16_t pe_machine(const std::filesystem::path& file) {
    std::ifstream f(file, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    if (!f.read(reinterpret_cast<char*>(&dos), sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    f.seekg(dos.e_lfanew);
    DWORD sig = 0;
    IMAGE_FILE_HEADER fh{};
    if (!f.read(reinterpret_cast<char*>(&sig), sizeof(sig)) || sig != IMAGE_NT_SIGNATURE) return 0;
    if (!f.read(reinterpret_cast<char*>(&fh), sizeof(fh))) return 0;
    return fh.Machine;
}

} // namespace omvr::platform::win32
