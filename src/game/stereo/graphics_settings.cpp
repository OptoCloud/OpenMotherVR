#include "game/stereo/graphics_settings.h"

#include "game/code_address.h"

#include <cstdint>
#include <utility>

namespace omvr::game::stereo {

namespace {

using SetOptionFn = void(__fastcall*)(void* settings, void* edx, std::int32_t option, const char* value);

} // namespace

void set_graphics_option(const AddressMap& addresses, core::stereo::GraphicsOption option,
                         const char* value) noexcept {
    const SetOptionFn set_option = CodeAddress{addresses.at(AiAddr::GraphicsSetOption)};
    void* settings               = addresses.at(AiAddr::GraphicsSettings);
    set_option(settings, nullptr, std::to_underlying(option), value);
}

} // namespace omvr::game::stereo
