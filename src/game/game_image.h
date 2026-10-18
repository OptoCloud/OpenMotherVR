// OpenMotherVR-Provenance: clean
#pragma once

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/constants.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace omvr::game {

// The running AI.exe as the installer sees it: where it is loaded, how big
// its image is, which build it is, and that build's addresses.
struct GameImage {
    std::uint8_t* base;
    std::uint32_t image_size;
    GameBuild version;
    const InlineSites& sites;
    const AddressMap& addresses;

    // The address of a named table slot.
    std::uint8_t* at(AiAddr slot) const noexcept { return addresses.at(slot); }

    // The address of an RVA of this image.
    std::uint8_t* at_rva(std::uint32_t rva) const noexcept { return base + rva; }

    // The RVA of an address inside this image.
    std::uint32_t rva_of(const std::uint8_t* p) const noexcept { return address_of(p) - address_of(base); }

    // `va` lies inside this image.
    bool contains(std::uint32_t va) const noexcept { return va - address_of(base) < image_size; }
};

// A vtable slot the patches check and write: slot `slot` of the vtable at
// `table` (a slot the table names itself is slot 0 of it), the stock method
// the preflight expects there (none: any code in AI.exe) and the name its
// log line gives the slot.
struct SlotSite {
    AiAddr table;
    std::uint32_t slot;
    std::optional<AiAddr> stock;
    std::string_view what;

    std::uint8_t* at(const GameImage& g) const noexcept { return g.at(table) + slot_offset(slot); }
};

} // namespace omvr::game
