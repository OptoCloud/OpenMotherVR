// OpenMotherVR-Provenance: clean
#pragma once

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/constants.h"

#include <cstdint>

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

} // namespace omvr::game
