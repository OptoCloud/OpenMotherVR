#pragma once

#include <chrono>

namespace omvr::core::ui {

// The lock the processes of one game launch share while one builds
// MOTHER.PAK, and how long another waits for it (spec 14 §5.8).
inline constexpr const wchar_t* kArchiveLockName = L"Local\\MotherVR.MotherPak";
inline constexpr std::chrono::milliseconds kArchiveLockWait{5000};

} // namespace omvr::core::ui
