// The "Stereo Mode == Rift" test rewrite.
//
// From: FUN_1001d6f0 @ 0x1001d6f0 lines 72..100, the one patch batch the
// original keeps outside its address table: two RVA arrays on the
// installer's stack, picked by AI.exe version, all written with the 2-byte
// payload at 0x1005dc34 (raw bytes and the installer listing are in the
// untracked archive/dumps/). Every other patch is in hook_installer.

#include "ai_patches.h"
#include "memory_patch.h"
#include "recon_addresses.h"

namespace mothervr::hook {

namespace {

// 1005dc34: `cmp eax, eax`, so ZF is always set: each `cmp reg, eax` that
// compares the "Stereo Mode" setting against "Rift" now always matches.
constexpr std::uint8_t kCmpEaxEax[2] = {0x39, 0xC0};

} // namespace

bool stereo_mode_tests_match(std::uint8_t* ai_base, integration::AIVersion version) noexcept {
    for (std::uint32_t rva : inline_sites(version).stereo_mode_tests) {
        const std::uint8_t* p = ai_base + rva;
        // 3B /r with mod=11, r/m=eax: `cmp reg, eax`.
        if (p[0] != 0x3B || (p[1] & 0xC7) != 0xC0) return false;
    }
    return true;
}

bool force_stereo_mode_tests(std::uint8_t* ai_base, integration::AIVersion version) noexcept {
    for (std::uint32_t rva : inline_sites(version).stereo_mode_tests) {
        if (!write_bytes(ai_base + rva, kCmpEaxEax, sizeof(kCmpEaxEax))) return false;
    }
    return true;
}

} // namespace mothervr::hook
