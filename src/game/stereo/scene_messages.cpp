#include "game/stereo/scene_messages.h"

#include "game/code_address.h"

namespace omvr::game::stereo {

namespace {

std::uint32_t hash_at(const AddressMap& addresses, AiAddr slot) noexcept { return read_u32(addresses.at(slot)); }

} // namespace

core::stereo::SceneMessages scene_messages(const AddressMap& addresses) noexcept {
    return {.cutscene_start  = hash_at(addresses, AiAddr::CutsceneVisibilityStart),
            .cutscene_finish = hash_at(addresses, AiAddr::CutsceneVisibilityFinish),
            .fade_start      = hash_at(addresses, AiAddr::LevelFadeStart),
            .fade_finish     = hash_at(addresses, AiAddr::LevelFadeFinish)};
}

} // namespace omvr::game::stereo
