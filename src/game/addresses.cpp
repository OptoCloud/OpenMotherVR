#include "game/addresses.h"

#include <algorithm>
#include <array>
#include <atomic>

namespace omvr::game {

namespace {

#include "game/ai_address_tables.inc"

std::atomic<const AddressMap*> g_process_addresses{nullptr};

} // namespace

void AddressMap::init(std::uint8_t* ai_base, GameBuild version) noexcept {
    base_ = ai_base;
    rvas_ = version == GameBuild::Patched_2020 ? &kAiRvas2020 : &kAiRvas2015;
}

std::uint8_t* AddressMap::at(AiAddr slot) const noexcept {
    return base_ + (*rvas_)[static_cast<std::size_t>(slot)];
}

bool AddressMap::all_within(std::uint32_t image_size) const noexcept {
    return std::ranges::all_of(*rvas_, [image_size](std::uint32_t rva) { return rva < image_size; });
}

void set_process_addresses(const AddressMap* map) noexcept {
    g_process_addresses.store(map, std::memory_order_release);
}

std::uint8_t* ai_address(AiAddr slot) noexcept {
    return g_process_addresses.load(std::memory_order_acquire)->at(slot);
}

} // namespace omvr::game
