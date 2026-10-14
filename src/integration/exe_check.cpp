// AI.exe version detection + this-module build-time extraction.
//
// From: inline code in FUN_10002250 @ 0x10002250 (DllMain) around
//       offsets +0x70..+0x150. The original used IMAGEHLP's MapAndLoad to
//       parse the PE on disk; that requires a working DllPath search path,
//       which is fragile. We just read the headers directly from the
//       already-loaded module: same information, no file I/O.

#include "exe_check.h"
#include <windows.h>

namespace mothervr::integration {

namespace {

// Read IMAGE_FILE_HEADER.TimeDateStamp from an already-loaded PE module.
// Returns nullopt if `hmod` is null or the headers don't look like PE.
std::optional<std::uint32_t> read_loaded_pe_timestamp(HMODULE hmod) noexcept {
    if (!hmod) return std::nullopt;
    auto* base = reinterpret_cast<const std::uint8_t*>(hmod);

    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return std::nullopt;

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return std::nullopt;

    return nt->FileHeader.TimeDateStamp;
}

// FILETIME representation of the Unix epoch (1970-01-01 00:00:00 UTC) in
// 100-ns intervals since 1601-01-01.
constexpr std::int64_t kFileTimeUnixEpoch = 0x019DB1DED53E8000LL;

} // namespace

std::optional<AIVersion> identify_ai_exe() {
    HMODULE hmod = ::GetModuleHandleA("AI.exe");
    if (!hmod) {
        // Some launches load the game as the main exe with a different name
        // (e.g. via Steam re-launch shims). Fall back to the process's own
        // main module.
        hmod = ::GetModuleHandleA(nullptr);
    }

    auto ts = read_loaded_pe_timestamp(hmod);
    if (!ts) return std::nullopt;

    switch (*ts) {
        case static_cast<std::uint32_t>(AIVersion::Original_2015): return AIVersion::Original_2015;
        case static_cast<std::uint32_t>(AIVersion::Patched_2020):  return AIVersion::Patched_2020;
        default: return std::nullopt;
    }
}

std::int64_t this_module_build_filetime() {
    HMODULE self = nullptr;
    ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                         | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(&this_module_build_filetime),
                         &self);
    auto ts = read_loaded_pe_timestamp(self);
    if (!ts) return 0;

    return static_cast<std::int64_t>(*ts) * 10'000'000LL + kFileTimeUnixEpoch;
}

// Diagnostic helper: returns the actual timestamp we read, regardless of
// whether it matched a known version. Used by the dialog message so the
// user can tell us *what* was found (the original's "Exe check failed! [0x%X]"
// format string).
std::uint32_t ai_exe_timestamp_raw() noexcept {
    HMODULE hmod = ::GetModuleHandleA("AI.exe");
    if (!hmod) hmod = ::GetModuleHandleA(nullptr);
    return read_loaded_pe_timestamp(hmod).value_or(0);
}

} // namespace mothervr::integration
