#pragma once

#include <cstdint>

#include "integration/exe_check.h"

namespace mothervr::hook {

// The eight "Stereo Mode == Rift" tests in AI.exe's stereo/render path
// (RVAs in InlineSites::stereo_mode_tests, recon_addresses.h). The hook
// installer's preflight checks each holds the game's `cmp reg, eax`.
//
// Rewrite each test as `cmp eax, eax` so it always passes and the game takes
// its LibOVR stereo path, which our stereo manager stands in for. Returns
// false if a write failed.
// From: FUN_1001d6f0 lines 72..100 (payload g_payload_1005dc34).
bool force_stereo_mode_tests(std::uint8_t* ai_base, integration::AIVersion version) noexcept;

} // namespace mothervr::hook
