// OpenMotherVR-Provenance: clean
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// The DXBC container and Shader Model 4/5 program tokens, as the D3D10/11
// token format (d3d11TokenizedProgramFormat.hpp) defines them: the parts
// the shader fixes read and write (08 §1).
namespace omvr::core::render {

// ---- The container --------------------------------------------------------------

// "DXBC", the checksum, a version, the total size, the chunk count and the
// chunk offsets, all little-endian 32-bit.
inline constexpr std::string_view kDxbcMagic = "DXBC";
inline constexpr std::size_t kDxbcMagicBytes = 4;
inline constexpr std::size_t kChecksumOffset = 4;
inline constexpr std::size_t kChecksumBytes  = 16;
// The checksum covers everything after itself and the version.
inline constexpr std::size_t kChecksummedFrom     = 20;
inline constexpr std::size_t kTotalSizeOffset     = 24;
inline constexpr std::size_t kChunkCountOffset    = 28;
inline constexpr std::size_t kChunkOffsetsOffset  = 32;
inline constexpr std::size_t kContainerHeaderSize = 32;
// A chunk: its four-character code, its size, then its data. The program
// chunk is SHEX (Shader Model 5) or SHDR (Shader Model 4).
inline constexpr std::string_view kProgramChunkSm5 = "SHEX";
inline constexpr std::string_view kProgramChunkSm4 = "SHDR";
inline constexpr std::size_t kFourCcBytes          = 4;
inline constexpr std::size_t kChunkSizeOffset      = 4;
inline constexpr std::size_t kChunkHeaderSize      = 8;
// Bytes of a token and of a container field.
inline constexpr std::size_t kTokenBytes = sizeof(std::uint32_t);

// ---- The program ----------------------------------------------------------------

// Token 0 is the version (the program type in its high half, 0 for a pixel
// shader), token 1 the program's length in tokens.
inline constexpr std::size_t kProgramHeaderTokens   = 2;
inline constexpr std::uint32_t kProgramTypeShift    = 16;
inline constexpr std::uint32_t kProgramTypeMask     = 0xFFFF;
inline constexpr std::uint32_t kPixelShaderProgram  = 0;
inline constexpr std::uint32_t kVertexShaderProgram = 1;

// Opcode token: opcode, length in tokens, extended flag.
inline constexpr std::uint32_t kOpMad = 0x32, kOpMul = 0x38, kOpNop = 0x3A, kOpCustomData = 0x35,
                               kOpDclConstantBuffer = 0x59;
// The fraction the dither fix takes of a wrapped position (08 §1.4 r7).
inline constexpr std::uint32_t kOpFrc = 0x1A;
// The arithmetic the upright-sprite fix inserts, and the declarations it
// reads or changes.
inline constexpr std::uint32_t kOpAdd = 0x00, kOpDp2 = 0x0F, kOpDp4 = 0x11, kOpLt = 0x31, kOpMax = 0x34,
                               kOpMov = 0x36, kOpMovc = 0x37, kOpRsq = 0x44, kOpSqrt = 0x4B,
                               kOpDclTemps = 0x68;

// The texture sample the constant smoke lighting replaces with a move (24
// §15), and the declaration of a texture.
inline constexpr std::uint32_t kOpSample = 0x45, kOpDclResource = 0x58;
// The declarations: SM4's range, SM5's range and its last one; they read no
// register as an instruction does.
inline constexpr std::uint32_t kOpFirstSm4Declaration = 0x58, kOpLastSm4Declaration = 0x6A,
                               kOpFirstSm5Declaration = 0x8F, kOpLastSm5Declaration = 0xA2,
                               kOpDclGsInstanceCount = 0xCE;

inline constexpr std::uint32_t kOpcodeMask   = 0x7FF;
inline constexpr std::uint32_t kLengthShift  = 24;
inline constexpr std::uint32_t kLengthMask   = 0x7F;
inline constexpr std::uint32_t kExtendedFlag = 0x80000000u;

// Operand token: component count, selection mode, mask or swizzle, operand
// type, index dimension and each index's representation.
inline constexpr std::uint32_t kComponentCountMask = 0x3;
inline constexpr std::uint32_t kFourComponents     = 2;
inline constexpr std::uint32_t kSelectionShift     = 2;
inline constexpr std::uint32_t kSelectionMaskBits  = 0x3;
inline constexpr std::uint32_t kSelectMask         = 0; // a write mask follows
inline constexpr std::uint32_t kSelectSwizzle      = 1; // a swizzle follows
inline constexpr std::uint32_t kSelect1            = 2; // one component follows
inline constexpr std::uint32_t kOneComponent       = 1; // a scalar immediate
inline constexpr std::uint32_t kComponentShift     = 4;
inline constexpr std::uint32_t kWriteMaskBits      = 0xF;
inline constexpr std::uint32_t kSwizzleBits        = 2; // per component
inline constexpr std::uint32_t kSwizzleMask        = 0x3;
inline constexpr std::uint32_t kTypeShift          = 12;
inline constexpr std::uint32_t kTypeMask           = 0xFF;
inline constexpr std::uint32_t kIndexDimShift      = 20;
inline constexpr std::uint32_t kIndexDimMask       = 0x3;
inline constexpr std::uint32_t kIndex1D            = 1;
inline constexpr std::uint32_t kIndex2D            = 2;
inline constexpr std::uint32_t kIndexRepShift      = 22;
inline constexpr std::uint32_t kIndexRepBits       = 3; // per index
inline constexpr std::uint32_t kIndexRepMask       = 0x7;
inline constexpr std::uint32_t kIndexImmediate32   = 0;

// Operand types.
inline constexpr std::uint32_t kTypeTemp = 0, kTypeInput = 1, kTypeImmediate32 = 4, kTypeSampler = 6,
                               kTypeResource = 7, kTypeConstantBuffer = 8;

// The extended opcode token: its type (low 6 bits), and for a resource
// dimension token the dimension (5 bits from bit 6). A chained token has
// bit 31 set.
inline constexpr std::uint32_t kExtendedTypeMask       = 0x3F;
inline constexpr std::uint32_t kExtendedResourceDim    = 2;
inline constexpr std::uint32_t kExtendedResourceReturn = 3;
inline constexpr std::uint32_t kExtendedDimShift       = 6;
inline constexpr std::uint32_t kExtendedDimMask        = 0x1F;
inline constexpr std::uint32_t kResourceDimTexture2D   = 3;
// The saturate flag of an opcode token.
inline constexpr std::uint32_t kSaturateFlag = 1u << 13;

// Components and write-mask bits.
inline constexpr std::uint32_t kX = 0, kY = 1, kZ = 2, kW = 3;
inline constexpr std::uint32_t kMaskXY = 0x3;

// The extended operand token of a negated source (modifier type 1, negate).
inline constexpr std::uint32_t kNegateModifier = 0x41;

// 0.001f as a 32-bit immediate.
inline constexpr std::uint32_t kPoint001 = 0x3A83126F;

// The program type of a version token (kPixelShaderProgram, ...).
inline std::uint32_t program_type(std::uint32_t tok) { return (tok >> kProgramTypeShift) & kProgramTypeMask; }
inline std::uint32_t opcode(std::uint32_t tok) { return tok & kOpcodeMask; }
inline std::uint32_t inst_length(std::uint32_t tok) { return (tok >> kLengthShift) & kLengthMask; }
inline std::uint32_t op_type(std::uint32_t tok) { return (tok >> kTypeShift) & kTypeMask; }
inline std::uint32_t index_dim(std::uint32_t tok) { return (tok >> kIndexDimShift) & kIndexDimMask; }
// An extended opcode or operand token (bit 31 of either).
inline bool op_extended(std::uint32_t tok) { return (tok & kExtendedFlag) != 0; }
inline std::uint32_t selection_mode(std::uint32_t tok) {
    return (tok >> kSelectionShift) & kSelectionMaskBits;
}
inline std::uint32_t index_rep(std::uint32_t tok, std::uint32_t dim) {
    return (tok >> (kIndexRepShift + kIndexRepBits * dim)) & kIndexRepMask;
}

// `tok` with its instruction length set to `length`.
inline std::uint32_t with_length(std::uint32_t tok, std::uint32_t length) {
    return (tok & ~(kLengthMask << kLengthShift)) | (length << kLengthShift);
}

// `tok` with its operand type set to `type`.
inline std::uint32_t with_type(std::uint32_t tok, std::uint32_t type) {
    return (tok & ~(kTypeMask << kTypeShift)) | (type << kTypeShift);
}

// A 4-component swizzle.
constexpr std::uint32_t swizzle(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t w) {
    return x | (y << kSwizzleBits) | (z << (2 * kSwizzleBits)) | (w << (3 * kSwizzleBits));
}

// A 4-component destination operand token with a write mask and immediate
// indices.
constexpr std::uint32_t masked_destination(std::uint32_t mask, std::uint32_t type, std::uint32_t index_dim) {
    return kFourComponents | (kSelectMask << kSelectionShift) | (mask << kComponentShift) |
           (type << kTypeShift) | (index_dim << kIndexDimShift);
}

// A 4-component source operand token selecting one component.
constexpr std::uint32_t selected_source(std::uint32_t component, std::uint32_t type,
                                        std::uint32_t index_dim) {
    return kFourComponents | (kSelect1 << kSelectionShift) | (component << kComponentShift) |
           (type << kTypeShift) | (index_dim << kIndexDimShift);
}

// An opcode token of `length` tokens.
constexpr std::uint32_t opcode_token(std::uint32_t op, std::uint32_t length) {
    return op | (length << kLengthShift);
}

// A 4-component source operand token with a swizzle and immediate indices.
constexpr std::uint32_t swizzled_source(std::uint32_t swz, std::uint32_t type, std::uint32_t index_dim) {
    return kFourComponents | (kSelectSwizzle << kSelectionShift) | (swz << kComponentShift) |
           (type << kTypeShift) | (index_dim << kIndexDimShift);
}

} // namespace omvr::core::render
