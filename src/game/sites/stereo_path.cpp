// The stereo path of the hook installer (FUN_1001d6f0 steps 6 and 7).
//
// The "Stereo Mode == Rift" test rewrite: from FUN_1001d6f0 @ 0x1001d6f0
// lines 72..100, the one patch batch the original keeps outside its address
// table: two RVA arrays on the installer's stack, picked by AI.exe version,
// all written with the 2-byte payload at 0x1005dc34 (raw bytes and the
// installer listing are in the untracked archive/dumps/).

#include "game/sites/stereo_path.h"

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "game/addresses.h"
#include "game/patcher.h"

namespace omvr::game::sites {

namespace {

namespace diag = core::diag;

// 1005dc34: `cmp eax, eax`, so ZF is always set: each `cmp reg, eax` that
// compares the "Stereo Mode" setting against "Rift" now always matches.
constexpr std::uint8_t kCmpEaxEax[2] = {0x39, 0xC0};

} // namespace

bool force_stereo_mode_tests(std::uint8_t* ai_base, GameBuild version) noexcept {
    for (std::uint32_t rva : inline_sites(version).stereo_mode_tests) {
        if (!write_bytes(ai_base + rva, kCmpEaxEax, sizeof(kCmpEaxEax))) return false;
    }
    return true;
}

bool force_stereo_mode(const GameImage& g) noexcept {
    if (!force_stereo_mode_tests(g.base, g.version)) return false;
    diag::log_line("[hook] stereo/render patches applied");
    return true;
}

bool install_stereo_device(StereoManager& m, const StereoDeviceHooks& hooks) noexcept {
    stereo::initialize(m, hooks.device, hooks.manager);

    // `push 0xb0; ...; call new; ...; call ctor` becomes
    // `mov eax, &manager; jmp short` to the store of the result.
    std::uint8_t* alloc = ai_address(AiAddr::StereoManagerAlloc);
    // Two recenter sites call device slot 11 (+0x2C); make them call
    // slot 21 (+0x54), the yaw-only reference pose.
    constexpr std::uint8_t kSlot21 = 0x54;
    const bool ok =
        patch(alloc, {0xB8}) && write_dword(alloc + 1, address_of(&m)) &&
        patch(alloc + 5, {0xEB, 0x1D}) &&
        patch(ai_address(AiAddr::HeadPoseCallA) + 2, {kSlot21}) &&
        patch(ai_address(AiAddr::HeadPoseCallB) + 2, {kSlot21}) &&
        redirect_call(ai_address(AiAddr::CreateDeviceCall), hooks.create_device, "CreateDevice") &&
        redirect_call(ai_address(AiAddr::HmdResolutionCall), hooks.get_hmd_resolution, "GetHmdResolution") &&
        redirect_call(ai_address(AiAddr::RenderTargetSizeCall), hooks.get_render_target_size,
                      "GetRenderTargetSize");
    if (ok) {
        diag::logf("[hook] stereo manager at {} handed to the game", static_cast<void*>(&m));
    }
    return ok;
}

} // namespace omvr::game::sites
