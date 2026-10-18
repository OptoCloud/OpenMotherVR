// OpenMotherVR-Provenance: clean
// The 2D-only effects in stereo (spec 24 §8): one call redirected per
// toggle, each checked right before it is written and off on its own.

#include "game/sites/post_fx_effects.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/sites/render_fixes.h"
#include "game/x86.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace omvr::game::sites {

namespace {

namespace ports = core::diag::ports;

constexpr std::string_view kNoAddress = "this build has no address for it";

constexpr std::string_view kKeyOnLog = "[hook] {}: off; Graphics.{} is true, the game's effect is kept in VR";
constexpr std::string_view kLensFlareLog = "[hook] {}: off; unexpected code at LensFlareSite (RVA {:08X})";
constexpr std::string_view kAberrationLog =
    "[hook] {}: off; unexpected code at ChromaticAberrationCall (RVA {:08X}) "
    "or in ChromaticAberration (RVA {:08X})";
constexpr std::string_view kFilmGrainLog =
    "[hook] {}: off; unexpected code at FilmGrainCall (RVA {:08X}) or in "
    "FilmGrain (RVA {:08X})";

constexpr int kSitesPerToggle = 1;

// ---- The code the toggles expect (24 §1.6, §1.8) ------------------------------

// ModR/M bytes of the register moves before the calls.
constexpr std::uint8_t kModRmEcxEax = 0xC8; // mov ecx, eax
constexpr std::uint8_t kModRmEcxEbx = 0xCB; // mov ecx, ebx
constexpr std::uint8_t kModRmEcxEsi = 0xCE; // mov ecx, esi
// `cmp r/m8, imm8` with [esi + disp32] (group 1, reg field 7).
constexpr std::uint8_t kModRmCmpEsiDisp32 = 0xBE;
// `movss xmm1, [esi + disp32]`.
constexpr std::uint8_t kModRmXmm1EsiDisp32 = 0x8E;
// `sub esp, imm8` (group 1 with a sign-extended imm8, reg field 5).
constexpr std::uint8_t kGroup1Imm8Signed = 0x83;
constexpr std::uint8_t kModRmSubEsp      = 0xEC;
// `test r/m8, imm8` with [disp32].
constexpr std::uint8_t kTestRm8Imm8 = 0xF6;

// The lens flare draw's pass in the post-processing render.
constexpr std::uint8_t kPostProcessingPass = 1;

// `push 1; call LensFlareGet; mov ecx, eax; call LensFlareDraw`: the get's
// call follows the push, the draw's the move.
constexpr std::size_t kLensFlareGetCall = x86::kPushImm8Size;
constexpr std::size_t kLensFlareMove    = kLensFlareGetCall + x86::kCallRel32Size;
constexpr std::array<std::uint8_t, 2> kPushPass{x86::kPushImm8, kPostProcessingPass};
constexpr std::array<std::uint8_t, 2> kMovEcxEax{x86::kMovR32Rm32, kModRmEcxEax};
constexpr std::size_t kLensFlareDrawCall = kLensFlareMove + kMovEcxEax.size();

// The register move before the readers' calls: the object in ecx.
constexpr std::array<std::uint8_t, 2> kMovEcxEbx{x86::kMovR32Rm32, kModRmEcxEbx};
constexpr std::array<std::uint8_t, 2> kMovEcxEsi{x86::kMovR32Rm32, kModRmEcxEsi};

constexpr std::uint8_t byte_of(std::ptrdiff_t value, int index) noexcept {
    return static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> (index * 8));
}

// ChromaticAberration's entry, `sub esp, 0x10; movss xmm0, [abs]` (the
// absolute's low byte, which relocation leaves alone, included), and its
// read of the flag, `cmp byte ptr [esi+0x1B38], 0`.
constexpr std::array<std::uint8_t, 8> kAberrationPrologue{kGroup1Imm8Signed, kModRmSubEsp,  0x10,
                                                          x86::kRepPrefix,   x86::kTwoByte, x86::kMovss,
                                                          x86::kModAbsolute, 0x14};
constexpr std::array<std::uint8_t, 7> kAberrationRead{x86::kGroup1Imm8,
                                                      kModRmCmpEsiDisp32,
                                                      byte_of(stereo::kPostFxChromaticAberration, 0),
                                                      byte_of(stereo::kPostFxChromaticAberration, 1),
                                                      byte_of(stereo::kPostFxChromaticAberration, 2),
                                                      byte_of(stereo::kPostFxChromaticAberration, 3),
                                                      0x00};

// FilmGrain's entry, `sub esp, 0x30; test byte ptr [abs], ...` (the
// absolute's low byte included), and its read of the intensity, `movss
// xmm1, [esi+0x608]`.
constexpr std::array<std::uint8_t, 6> kFilmGrainPrologue{kGroup1Imm8Signed, kModRmSubEsp,      0x30,
                                                         kTestRm8Imm8,      x86::kModAbsolute, 0xCC};
constexpr std::array<std::uint8_t, 8> kFilmGrainRead{x86::kRepPrefix,
                                                     x86::kTwoByte,
                                                     x86::kMovss,
                                                     kModRmXmm1EsiDisp32,
                                                     byte_of(stereo::kPostFxFilmGrain, 0),
                                                     byte_of(stereo::kPostFxFilmGrain, 1),
                                                     byte_of(stereo::kPostFxFilmGrain, 2),
                                                     byte_of(stereo::kPostFxFilmGrain, 3)};

// ---- One function per toggle ----------------------------------------------------

// Lens flares (24 §8): the post-processing render's draw (pass 1, flat and
// stereo); the flat-only draw at the end of the final post chain is left
// alone.
void toggle_lens_flares(const GameImage& g, Patcher& p, const PostFxEffectRvas& r,
                        const PostFxEffectHooks& hooks) noexcept {
    const std::uint8_t* site = g.at_rva(r.lens_flare_site);
    if (!x86::matches(site, kPushPass) || !calls(site + kLensFlareGetCall, g.at_rva(r.lens_flare_get)) ||
        !x86::matches(site + kLensFlareMove, kMovEcxEax) ||
        !calls(site + kLensFlareDrawCall, g.at_rva(r.lens_flare_draw))) {
        ports::logf(p.log(), kLensFlareLog, kLensFlareToggleArea, r.lens_flare_site);
        return;
    }
    hooks.targets->lens_flare_draw = g.at_rva(r.lens_flare_draw);
    if (p.redirect_call(g.at_rva(r.lens_flare_site) + kLensFlareDrawCall, hooks.lens_flare_draw,
                        "LensFlareDraw")) {
        area_on(p.log(), kLensFlareToggleArea, kSitesPerToggle);
    }
}

// Chromatic aberration (24 §8): its reader, run with the flag off.
void toggle_chromatic_aberration(const GameImage& g, Patcher& p, const PostFxEffectRvas& r,
                                 const PostFxEffectHooks& hooks) noexcept {
    std::uint8_t* call = g.at_rva(r.chromatic_aberration_call);
    if (!x86::matches(call - kMovEcxEbx.size(), kMovEcxEbx) ||
        !calls(call, g.at_rva(r.chromatic_aberration)) ||
        !x86::matches(g.at_rva(r.chromatic_aberration), kAberrationPrologue) ||
        !x86::matches(g.at_rva(r.chromatic_aberration_read), kAberrationRead)) {
        ports::logf(p.log(), kAberrationLog, kChromaticAberrationToggleArea, r.chromatic_aberration_call,
                    r.chromatic_aberration);
        return;
    }
    hooks.targets->chromatic_aberration = g.at_rva(r.chromatic_aberration);
    if (p.redirect_call(call, hooks.chromatic_aberration, "ChromaticAberration")) {
        area_on(p.log(), kChromaticAberrationToggleArea, kSitesPerToggle);
    }
}

// Film grain (24 §8): its reader, run with intensity 0.
void toggle_film_grain(const GameImage& g, Patcher& p, const PostFxEffectRvas& r,
                       const PostFxEffectHooks& hooks) noexcept {
    std::uint8_t* call = g.at_rva(r.film_grain_call);
    if (!x86::matches(call - kMovEcxEsi.size(), kMovEcxEsi) || !calls(call, g.at_rva(r.film_grain)) ||
        !x86::matches(g.at_rva(r.film_grain), kFilmGrainPrologue) ||
        !x86::matches(g.at_rva(r.film_grain_read), kFilmGrainRead)) {
        ports::logf(p.log(), kFilmGrainLog, kFilmGrainToggleArea, r.film_grain_call, r.film_grain);
        return;
    }
    hooks.targets->film_grain = g.at_rva(r.film_grain);
    if (p.redirect_call(call, hooks.film_grain, "FilmGrain")) {
        area_on(p.log(), kFilmGrainToggleArea, kSitesPerToggle);
    }
}

using Toggle = void (*)(const GameImage&, Patcher&, const PostFxEffectRvas&,
                        const PostFxEffectHooks&) noexcept;

// Installs `toggle` when its key is false and this build has its address
// (`rva`); otherwise reports why it is not.
void toggle_if(bool key, std::string_view key_name, std::uint32_t rva, std::string_view area, Toggle toggle,
               const GameImage& g, Patcher& p, const PostFxEffectRvas& r,
               const PostFxEffectHooks& hooks) noexcept {
    if (key) {
        ports::logf(p.log(), kKeyOnLog, area, key_name);
    } else if (rva == 0) {
        area_off(p.log(), area, kNoAddress);
    } else {
        toggle(g, p, r, hooks);
    }
}

} // namespace

void install_post_fx_effects(const GameImage& g, Patcher& p, const PostFxEffectHooks& hooks,
                             const core::render::PostFxKeys& keys) noexcept {
    const PostFxEffectRvas& r = kPostFxEffectRvas[g.version];
    toggle_if(keys.lens_flares, "LensFlares", r.lens_flare_site, kLensFlareToggleArea, &toggle_lens_flares, g,
              p, r, hooks);
    toggle_if(keys.chromatic_aberration, "ChromaticAberration", r.chromatic_aberration_call,
              kChromaticAberrationToggleArea, &toggle_chromatic_aberration, g, p, r, hooks);
    toggle_if(keys.film_grain, "FilmGrain", r.film_grain_call, kFilmGrainToggleArea, &toggle_film_grain, g, p,
              r, hooks);
}

} // namespace omvr::game::sites
