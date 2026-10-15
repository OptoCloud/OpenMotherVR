#pragma once

#include <cstddef>
#include <cstdint>

// Values the Windows API defines, or that the mod decides about Windows
// files and processes, for code that does not include <windows.h>.
namespace omvr::platform::win32 {

// FILETIME of the Unix epoch (1970-01-01 00:00:00 UTC): 100-ns intervals
// since 1601-01-01.
inline constexpr std::int64_t kFileTimeUnixEpoch = 0x019DB1DED53E8000LL;

// FILETIME ticks (100 ns) per second.
inline constexpr std::int64_t kFileTimeTicksPerSecond = 10'000'000LL;

// A FILETIME and a file size come as two DWORD halves; the high half is
// shifted by this many bits.
inline constexpr unsigned kDwordBits = 32;

// MAX_PATH, for buffers declared in headers that do not include windows.h.
inline constexpr std::size_t kMaxPath = 260;

// UTF-8 needs at most this many bytes for one UTF-16 code unit (a surrogate
// pair, two units, takes four), so a name of N units fits in N * 3 bytes.
inline constexpr std::size_t kUtf8BytesPerUtf16Unit = 3;

// A path buffer that was too small grows by this factor and is retried.
inline constexpr std::size_t kPathBufferGrowth = 2;

// The resource type of the payloads embedded in the DLL (resources.rc).
inline constexpr const wchar_t* kBinaryResourceType = L"BINARY";

// The id of the module's version resource (version.rc.in: `1 VERSIONINFO`).
inline constexpr int kVersionResourceId = 1;

// The children of a VS_VERSIONINFO block start on DWORD boundaries, so the
// fixed part is searched for at this step.
inline constexpr std::size_t kVersionInfoAlignment = 4;

// The file the embedded OpenVR API is written to, next to this DLL, where
// the OpenVR runtime loads it from.
inline constexpr const wchar_t* kOpenVrApiFileName = L"openvr_api.dll";

// Steam sets one of these in the environment of the game process it starts
// (spec 11 §1.8: the "Game" process; without them, the "Launcher").
inline constexpr const wchar_t* kSteamGameIdVariable = L"SteamGameId";
inline constexpr const wchar_t* kSteamAppIdVariable  = L"SteamAppId";

// GetAsyncKeyState's result has this bit set while the key is down.
inline constexpr std::int16_t kKeyDownBit = static_cast<std::int16_t>(0x8000);

// RtlGetVersion reports the real Windows build, unlike GetVersionEx, whose
// answer depends on what the game's manifest claims to support.
inline constexpr const wchar_t* kNtdllFileName  = L"ntdll.dll";
inline constexpr const char* kRtlGetVersionName = "RtlGetVersion";

// The crash record names the modules of this many words on top of the
// faulting thread's stack (the return addresses of its callers).
inline constexpr int kCrashStackWords = 16;

// EXCEPTION_RECORD::ExceptionInformation[0] of an access violation: the
// kind of access (any other value is a data execution prevention fault).
inline constexpr std::uintptr_t kAccessViolationRead  = 0;
inline constexpr std::uintptr_t kAccessViolationWrite = 1;

} // namespace omvr::platform::win32
