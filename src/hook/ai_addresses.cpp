#include "ai_addresses.h"

#include <algorithm>
#include <array>

namespace mothervr::hook {

namespace {

#include "ai_address_tables.inc"

std::uint8_t*                        g_base = nullptr;
const std::array<std::uint32_t, 127>* g_rvas = nullptr;

} // namespace

void init_ai_addresses(std::uint8_t* ai_base, integration::AIVersion version) noexcept {
    g_base = ai_base;
    g_rvas = version == integration::AIVersion::Patched_2020 ? &kAiRvas2020 : &kAiRvas2015;
}

std::uint8_t* ai_address(AiAddr slot) noexcept {
    return g_base + (*g_rvas)[static_cast<std::size_t>(slot)];
}

bool ai_addresses_within(std::uint32_t image_size) noexcept {
    return std::ranges::all_of(*g_rvas, [image_size](std::uint32_t rva) { return rva < image_size; });
}

} // namespace mothervr::hook
