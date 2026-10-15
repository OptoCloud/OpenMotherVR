#pragma once

#include <cstdint>

// Shader model 4/5 program tokens, as the D3D10/11 token format defines them.
namespace omvr::core::render {

inline constexpr std::uint32_t kOpMad = 0x32, kOpMul = 0x38, kOpNop = 0x3A, kOpDclConstantBuffer = 0x59;
inline constexpr std::uint32_t kTypeTemp = 0, kTypeInput = 1, kTypeImmediate32 = 4, kTypeConstantBuffer = 8;
inline constexpr std::uint32_t kPoint001 = 0x3A83126F;   // 0.001f

inline std::uint32_t opcode(std::uint32_t tok) { return tok & 0x7FF; }
inline std::uint32_t inst_length(std::uint32_t tok) { return (tok >> 24) & 0x7F; }
inline std::uint32_t op_type(std::uint32_t tok) { return (tok >> 12) & 0xFF; }
inline std::uint32_t index_dim(std::uint32_t tok) { return (tok >> 20) & 0x3; }
inline bool op_extended(std::uint32_t tok) { return tok >> 31; }

} // namespace omvr::core::render
