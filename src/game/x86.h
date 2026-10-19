// OpenMotherVR-Provenance: clean
#pragma once

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

// The x86 (IA-32) encodings the patches check and write. Instruction-set
// facts from the Intel SDM, volume 2; nothing here knows about AI.exe.
namespace omvr::game::x86 {

// ---- Opcodes ---------------------------------------------------------------

inline constexpr std::uint8_t kCallRel32        = 0xE8; // call rel32
inline constexpr std::uint8_t kJmpRel32         = 0xE9; // jmp rel32
inline constexpr std::uint8_t kJmpRel8          = 0xEB; // jmp short rel8
inline constexpr std::uint8_t kJeRel8           = 0x74; // je short rel8
inline constexpr std::uint8_t kJneRel8          = 0x75; // jne short rel8
inline constexpr std::uint8_t kJbeRel8          = 0x76; // jbe short rel8
inline constexpr std::uint8_t kMovEaxMoffs      = 0xA1; // mov eax, [moffs32]
inline constexpr std::uint8_t kNop              = 0x90;
inline constexpr std::uint8_t kRet              = 0xC3;
inline constexpr std::uint8_t kPushImm32        = 0x68; // push imm32
inline constexpr std::uint8_t kPushImm8         = 0x6A; // push imm8
inline constexpr std::uint8_t kPushEbp          = 0x55;
inline constexpr std::uint8_t kPushEcx          = 0x51;
inline constexpr std::uint8_t kPushEbx          = 0x53;
inline constexpr std::uint8_t kPushEax          = 0x50;
inline constexpr std::uint8_t kPushEsi          = 0x56;
inline constexpr std::uint8_t kMovEaxImm32      = 0xB8; // mov eax, imm32
inline constexpr std::uint8_t kMovClImm8        = 0xB1; // mov cl, imm8
inline constexpr std::uint8_t kMovRm8R8         = 0x88; // mov r/m8, r8
inline constexpr std::uint8_t kMovRm8Imm8       = 0xC6; // mov r/m8, imm8
inline constexpr std::uint8_t kMovRm32R32       = 0x89; // mov r/m32, r32
inline constexpr std::uint8_t kMovR8Rm8         = 0x8A; // mov r8, r/m8
inline constexpr std::uint8_t kMovR32Rm32       = 0x8B; // mov r32, r/m32
inline constexpr std::uint8_t kCmpR32Rm32       = 0x3B; // cmp r32, r/m32
inline constexpr std::uint8_t kCmpRm32R32       = 0x39; // cmp r/m32, r32
inline constexpr std::uint8_t kLea              = 0x8D; // lea r32, m
inline constexpr std::uint8_t kGroup1Imm8       = 0x80; // cmp/add/... r/m8, imm8 (reg field selects)
inline constexpr std::uint8_t kGroup1Imm32      = 0x81; // sub/add/... r/m32, imm32 (reg field selects)
inline constexpr std::uint8_t kGroup1Imm8Signed = 0x83; // sub/add/... r/m32, sign-extended imm8
inline constexpr std::uint8_t kRetImm16         = 0xC2; // ret imm16: pops imm16 bytes of arguments
inline constexpr std::uint8_t kTwoByte          = 0x0F; // escape to the two-byte opcode map

// Second bytes after kTwoByte.
inline constexpr std::uint8_t kJbeRel32 = 0x86; // jbe rel32
inline constexpr std::uint8_t kSetne    = 0x95; // setne r/m8
inline constexpr std::uint8_t kComiss   = 0x2F; // comiss xmm, xmm/m32
inline constexpr std::uint8_t kMovss    = 0x10; // movss xmm, xmm/m32 (after kRepPrefix)

inline constexpr std::uint8_t kRepPrefix = 0xF3; // selects movss over movups

// Opcode extensions in the reg field of the group 1 opcodes.
inline constexpr std::uint8_t kGroup1And = 4;
inline constexpr std::uint8_t kGroup1Sub = 5;
inline constexpr std::uint8_t kGroup1Cmp = 7;

// ---- Registers -------------------------------------------------------------

// The register numbers of the reg and r/m fields (al..bh in 8-bit forms).
enum class Reg : std::uint8_t { Eax, Ecx, Edx, Ebx, Esp, Ebp, Esi, Edi };

// ---- ModRM -----------------------------------------------------------------

inline constexpr std::uint8_t kModMask         = 0xC0; // the mod field
inline constexpr std::uint8_t kModRegMask      = 0xC7; // mod and r/m, any reg
inline constexpr std::uint8_t kModRegFieldMask = 0xF8; // mod and reg, any r/m
inline constexpr std::uint8_t kModRegister     = 0xC0; // mod 11: register operand
inline constexpr std::uint8_t kModDisp8        = 0x40; // mod 01: [reg + disp8]
inline constexpr std::uint8_t kModDisp32       = 0x80; // mod 10: [reg + disp32]; reg field 0 is eax/al
inline constexpr std::uint8_t kModAbsolute     = 0x05; // mod 00, r/m 101: [disp32]

inline constexpr unsigned kModRmRegShift = 3; // the reg field's position

// A ModRM byte from its mod bits (kModRegister, kModDisp8, ...), reg field
// (a register number or an opcode extension) and r/m field.
constexpr std::uint8_t modrm(std::uint8_t mod, std::uint8_t reg, std::uint8_t rm) noexcept {
    return static_cast<std::uint8_t>(mod | (reg << kModRmRegShift) | rm);
}

// The register form, `reg, rm`: for `mov r32, r/m32` (8B) the destination
// is `reg`.
constexpr std::uint8_t modrm_reg(Reg reg, Reg rm) noexcept {
    return modrm(kModRegister, std::to_underlying(reg), std::to_underlying(rm));
}

inline constexpr std::uint8_t kModRmEcxAbs = 0x0D; // ecx, [disp32]
inline constexpr std::uint8_t kModRmClSib  = 0x4C; // cl, [sib + disp8]
inline constexpr std::uint8_t kSibEsp      = 0x24; // [esp]
inline constexpr std::uint8_t kModRmEaxEax = modrm_reg(Reg::Eax, Reg::Eax);
// cl as the one operand (setcc): the reg field is unused, so this is the
// byte of `eax, ecx`.
inline constexpr std::uint8_t kModRmCl = modrm_reg(Reg::Eax, Reg::Ecx);
// `sub esp, imm` and `and esp, imm` (group 1 on esp).
inline constexpr std::uint8_t kModRmSubEsp = modrm(kModRegister, kGroup1Sub, std::to_underlying(Reg::Esp));
inline constexpr std::uint8_t kModRmAndEsp = modrm(kModRegister, kGroup1And, std::to_underlying(Reg::Esp));
static_assert(kModRmEaxEax == 0xC0 && kModRmCl == 0xC1 && kModRmSubEsp == 0xEC && kModRmAndEsp == 0xE4);

// ---- Instruction sizes -----------------------------------------------------

inline constexpr std::size_t kCallRel32Size    = 5; // E8 rel32
inline constexpr std::size_t kJmpRel32Size     = 5; // E9 rel32
inline constexpr std::size_t kJbeRel32Size     = 6; // 0F 86 rel32
inline constexpr std::size_t kJmpRel8Size      = 2; // EB rel8
inline constexpr std::size_t kMovEaxImm32Size  = 5; // B8 imm32
inline constexpr std::size_t kMovEcxAbsSize    = 6; // 8B 0D disp32
inline constexpr std::size_t kPushImm8Size     = 2; // 6A imm8
inline constexpr std::size_t kMovRm8Disp32Size = 6; // 88 modrm disp32
inline constexpr std::size_t kRel32Size        = 4; // the operand of a rel32 or disp32 form

// The operand of `call rel32` (and of `push imm32`, `mov eax, imm32`)
// follows its opcode byte.
inline constexpr std::size_t kCallOperandOffset = 1;
inline constexpr std::size_t kImm32Offset       = 1;

// Where the disp32 starts: byte 4 of `movss xmm, [disp32]` (F3 0F 10 modrm
// disp32), byte 3 of `comiss xmm, [disp32]` (0F 2F modrm disp32).
inline constexpr std::size_t kMovssDispOffset  = 4;
inline constexpr std::size_t kComissDispOffset = 3;

// `mov [reg+disp32], r8` (88 modrm disp32) and `mov r32, [reg+disp8]` (8B
// modrm disp8): the displacement at byte 2.
inline constexpr std::size_t kMovRmDispOffset = 2;

// The farthest a `jmp short` reaches forward.
inline constexpr std::size_t kMaxRel8 = 0x7F;

// ---- Byte sequences --------------------------------------------------------

// `a` followed by `b`.
template <std::size_t A, std::size_t B>
constexpr std::array<std::uint8_t, A + B> concat(const std::array<std::uint8_t, A>& a,
                                                 const std::array<std::uint8_t, B>& b) noexcept {
    std::array<std::uint8_t, A + B> out{};
    for (std::size_t i = 0; i < A; ++i) out[i] = a[i];
    for (std::size_t i = 0; i < B; ++i) out[A + i] = b[i];
    return out;
}

// `push ebp; mov ebp, esp`: a function's frame.
inline constexpr std::array<std::uint8_t, 3> kFramePrologue = {kPushEbp, kMovR32Rm32,
                                                               modrm_reg(Reg::Ebp, Reg::Esp)};

// `and esp, -16` after it: a frame with the stack aligned for SSE.
inline constexpr std::uint8_t kAlign16Imm8 = 0xF0; // -16 as a sign-extended imm8
inline constexpr auto kAlignedFramePrologue =
    concat(kFramePrologue, std::array<std::uint8_t, 3>{kGroup1Imm8Signed, kModRmAndEsp, kAlign16Imm8});
static_assert(kAlignedFramePrologue == std::array<std::uint8_t, 6>{0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0});

// ---- Patterns --------------------------------------------------------------

// Bytes to compare under a mask: a site matches when (byte & mask) equals
// `bytes` at every position. Register fields the game's compiler chose are
// masked out where the patch does not depend on them.
template <std::size_t N>
struct Pattern {
    std::array<std::uint8_t, N> bytes;
    std::array<std::uint8_t, N> mask;
};

template <std::size_t N>
bool matches(const std::uint8_t* site, const Pattern<N>& p) noexcept {
    for (std::size_t i = 0; i < N; ++i) {
        if ((site[i] & p.mask[i]) != p.bytes[i]) return false;
    }
    return true;
}

template <std::size_t N>
bool matches(const std::uint8_t* site, const std::array<std::uint8_t, N>& bytes) noexcept {
    return std::memcmp(site, bytes.data(), N) == 0;
}

// ---- Replacement code ------------------------------------------------------

// The little-endian bytes of a 32-bit operand.
constexpr std::array<std::uint8_t, kRel32Size> le32(std::uint32_t value) noexcept {
    std::array<std::uint8_t, kRel32Size> out{};
    for (std::size_t i = 0; i < kRel32Size; ++i) out[i] = static_cast<std::uint8_t>(value >> (i * CHAR_BIT));
    return out;
}

// `bytes` with the 32-bit operand at byte `At` set to `value`: the
// expected bytes of code whose operand is an absolute address, which the
// loader rebases with the image.
template <std::size_t At, std::size_t N>
    requires(At + kRel32Size <= N)
constexpr std::array<std::uint8_t, N> with_imm32(std::array<std::uint8_t, N> bytes,
                                                 std::uint32_t value) noexcept {
    const auto d = le32(value);
    for (std::size_t i = 0; i < kRel32Size; ++i) bytes[At + i] = d[i];
    return bytes;
}

// `jmp rel32`.
constexpr std::array<std::uint8_t, kJmpRel32Size> jmp_rel32(std::uint32_t rel) noexcept {
    const auto d = le32(rel);
    return {kJmpRel32, d[0], d[1], d[2], d[3]};
}

// `jbe rel32`.
constexpr std::array<std::uint8_t, kJbeRel32Size> jbe_rel32(std::uint32_t rel) noexcept {
    const auto d = le32(rel);
    return {kTwoByte, kJbeRel32, d[0], d[1], d[2], d[3]};
}

// N bytes of `nop`.
template <std::size_t N>
constexpr std::array<std::uint8_t, N> nops() noexcept {
    std::array<std::uint8_t, N> out{};
    out.fill(kNop);
    return out;
}

// N bytes that jump over themselves: `jmp short` past the rest, padded with
// `nop`. Removes an N-byte instruction sequence without moving the code
// after it.
template <std::size_t N>
constexpr std::array<std::uint8_t, N> jump_over() noexcept {
    static_assert(N >= kJmpRel8Size && N - kJmpRel8Size <= kMaxRel8, "a short jump reaches 127 bytes");
    auto out = nops<N>();
    out[0]   = kJmpRel8;
    out[1]   = static_cast<std::uint8_t>(N - kJmpRel8Size);
    return out;
}

} // namespace omvr::game::x86
