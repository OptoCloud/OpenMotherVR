#include "game/addresses.h"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace omvr::game {

namespace {

#include "game/ai_address_tables.inc"

static_assert(std::is_same_v<decltype(kAiRvas2015), const AddressTable>);
static_assert(std::is_same_v<decltype(kAiRvas2020), const AddressTable>);

} // namespace

void AddressMap::init(std::uint8_t* ai_base, GameBuild version) noexcept {
    base_ = ai_base;
    rvas_ = version == GameBuild::Patched2020 ? &kAiRvas2020 : &kAiRvas2015;
}

std::uint8_t* AddressMap::at(AiAddr slot) const noexcept {
    return base_ + (*rvas_)[std::to_underlying(slot)];
}

bool AddressMap::all_within(std::uint32_t image_size) const noexcept {
    return std::ranges::all_of(*rvas_, [image_size](std::uint32_t rva) { return rva < image_size; });
}

} // namespace omvr::game
