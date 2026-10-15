#include "platform/win32/patching/pe_image.h"

#include <windows.h>

#include <fstream>

namespace omvr::platform::win32 {

static_assert(kMachineI386 == IMAGE_FILE_MACHINE_I386);

namespace {

// The NT headers of the 32-bit image loaded at `base`, or null when the DOS
// or NT signature is missing.
const IMAGE_NT_HEADERS32* nt_headers(const std::uint8_t* base) noexcept {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE ? nt : nullptr;
}

template <class T>
bool read_struct(std::ifstream& f, T& out) {
    return static_cast<bool>(f.read(reinterpret_cast<char*>(&out), sizeof(out)));
}

} // namespace

std::optional<std::uint32_t> loaded_image_timestamp(const void* module) noexcept {
    if (!module) return std::nullopt;
    const auto* nt = nt_headers(static_cast<const std::uint8_t*>(module));
    if (!nt) return std::nullopt;
    return nt->FileHeader.TimeDateStamp;
}

std::uint32_t image_size_of(const std::uint8_t* base) noexcept {
    // The image is a module the loader mapped: its headers are valid.
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    return nt->OptionalHeader.SizeOfImage;
}

std::uint16_t pe_machine(const std::filesystem::path& file) {
    std::ifstream f(file, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    if (!read_struct(f, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    f.seekg(dos.e_lfanew);
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    if (!read_struct(f, signature) || signature != IMAGE_NT_SIGNATURE) return 0;
    if (!read_struct(f, header)) return 0;
    return header.Machine;
}

} // namespace omvr::platform::win32
