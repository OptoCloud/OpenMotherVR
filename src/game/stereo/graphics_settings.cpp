#include "game/stereo/graphics_settings.h"

#include "game/addresses.h"

namespace omvr::game::stereo {

void set_graphics_option(int option, const char* value) noexcept {
    using SetOptionFn = void (__fastcall*)(void* settings, void* edx, int option, const char* value);
    auto set_option = reinterpret_cast<SetOptionFn>(ai_address(AiAddr::GraphicsSetOption));
    void* settings  = ai_address(AiAddr::GraphicsSettings);
    set_option(settings, nullptr, option, value);
}

} // namespace omvr::game::stereo
