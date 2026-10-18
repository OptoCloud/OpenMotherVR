// The stereo path of the hook installer (FUN_1001d6f0 steps 6 and 7).
//
// The "Stereo Mode == Rift" test rewrite: from FUN_1001d6f0 @ 0x1001d6f0
// lines 72..100, the one patch batch the original keeps outside its address
// table: two RVA arrays on the installer's stack, picked by AI.exe version,
// all written with the 2-byte payload at 0x1005dc34 (raw bytes and the
// installer listing are in the untracked archive/dumps/).

#include "game/sites/stereo_path.h"

#include "game/code_address.h"
#include "game/x86.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace omvr::game::sites {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kStereoPatchesLog = "[hook] stereo/render patches applied";
constexpr std::string_view kHandedOverLog    = "[hook] stereo manager at {} handed to the game";

// 1005dc34: `cmp eax, eax`, so ZF is always set: each `cmp reg, eax` that
// compares the "Stereo Mode" setting against "Rift" now always matches.
constexpr std::array<std::uint8_t, 2> kCmpEaxEax = {x86::kCmpRm32R32, x86::kModRmEaxEax};

// The hand-off (02 §3.1): `push 0xb0; ...; call new; ...; call ctor`
// becomes `mov eax, &manager; jmp short` to the store of the result.
constexpr std::size_t kHandOffJumpSite                             = x86::kMovEaxImm32Size;
constexpr std::array<std::uint8_t, x86::kJmpRel8Size> kJumpToStore = {
    x86::kJmpRel8,
    static_cast<std::uint8_t>(kStereoAllocStoreOffset - (x86::kMovEaxImm32Size + x86::kJmpRel8Size))};
static_assert(kJumpToStore[1] == 0x1D, "the jump the hand-off has always written");
constexpr std::array<std::uint8_t, 1> kMovEax = {x86::kMovEaxImm32};

// The recenter sites call device slot 11 (+0x2C); they are made to call
// slot 21 (+0x54), the yaw-only reference pose (02 §6.4).
constexpr std::array<std::uint8_t, 1> kReferencePoseSlot = {
    static_cast<std::uint8_t>(slot_offset(kDeviceReferencePoseSlot))};

bool hand_off_manager(const GameImage& g, Patcher& p, StereoManager& m) noexcept {
    std::uint8_t* alloc = g.at(AiAddr::StereoManagerAlloc);
    return p.write(alloc, kMovEax) && p.write_pointer(alloc + x86::kImm32Offset, &m) &&
           p.write(alloc + kHandOffJumpSite, kJumpToStore);
}

bool point_recenter_sites(const GameImage& g, Patcher& p) noexcept {
    return p.write(g.at(AiAddr::HeadPoseCallA) + x86::kMovRmDispOffset, kReferencePoseSlot) &&
           p.write(g.at(AiAddr::HeadPoseCallB) + x86::kMovRmDispOffset, kReferencePoseSlot);
}

bool redirect_manager_calls(const GameImage& g, Patcher& p, const StereoDeviceHooks& hooks) noexcept {
    const std::array calls{
        CallRedirect{g.at(AiAddr::CreateDeviceCall), hooks.create_device, "CreateDevice"},
        CallRedirect{g.at(AiAddr::HmdResolutionCall), hooks.get_hmd_resolution, "GetHmdResolution"},
        CallRedirect{g.at(AiAddr::RenderTargetSizeCall), hooks.get_render_target_size,
                     "GetRenderTargetSize"}};
    return redirect_all(p, calls);
}

} // namespace

bool force_stereo_mode_tests(const GameImage& g, Patcher& p) noexcept {
    for (std::uint32_t rva : g.sites.stereo_mode_tests) {
        if (!p.write(g.at_rva(rva), kCmpEaxEax)) return false;
    }
    return true;
}

bool force_stereo_mode(const GameImage& g, Patcher& p) noexcept {
    if (!force_stereo_mode_tests(g, p)) return false;
    p.log().line(kStereoPatchesLog);
    return true;
}

bool install_stereo_device(const GameImage& g, Patcher& p, StereoManager& m,
                           const StereoDeviceHooks& hooks) noexcept {
    stereo::initialize(g.addresses, m, hooks.device, hooks.manager);
    const bool ok =
        hand_off_manager(g, p, m) && point_recenter_sites(g, p) && redirect_manager_calls(g, p, hooks);
    if (ok) ports::logf(p.log(), kHandedOverLog, static_cast<void*>(&m));
    return ok;
}

} // namespace omvr::game::sites
