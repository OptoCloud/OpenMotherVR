// OpenMotherVR-Provenance: clean
#include "platform/win32/patching/import_slot.h"

#include <cstring>
#include <windows.h>

namespace omvr::platform::win32 {

namespace {

bool same_dll(const char* a, std::string_view b) noexcept {
    return std::strlen(a) == b.size() && ::_strnicmp(a, b.data(), b.size()) == 0;
}

} // namespace

void** import_slot(void* module, std::string_view dll, std::string_view function) noexcept {
    if (!module) return nullptr;
    auto* base      = static_cast<std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT)
        return nullptr;
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (dir.VirtualAddress == 0) return nullptr;

    for (auto* desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); desc->Name;
         ++desc) {
        if (!same_dll(reinterpret_cast<const char*>(base + desc->Name), dll)) continue;
        // The name table; without one (an old bound image) the IAT itself
        // still holds the names until the loader overwrote them, so give up.
        if (desc->OriginalFirstThunk == 0) return nullptr;
        const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA32*>(base + desc->OriginalFirstThunk);
        auto* slots       = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + desc->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)) continue;
            const auto* by_name =
                reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            const char* name = reinterpret_cast<const char*>(by_name->Name);
            if (std::strlen(name) == function.size() &&
                std::memcmp(name, function.data(), function.size()) == 0)
                return reinterpret_cast<void**>(&slots->u1.Function);
        }
    }
    return nullptr;
}

} // namespace omvr::platform::win32
