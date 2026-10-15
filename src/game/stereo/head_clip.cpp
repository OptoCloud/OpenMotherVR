// OpenMotherVR-Provenance: clean
#include "game/stereo/head_clip.h"

#include "game/code_address.h"

namespace omvr::game::stereo {

namespace {

using HeadClipFadeFn = float(__stdcall*)(const float* eye, const float* pivot);

} // namespace

float call_head_clip_fade(const HeadClipTargets& targets, const float* eye, const float* pivot) noexcept {
    const HeadClipFadeFn fade = CodeAddress{targets.fade};
    return fade(eye, pivot);
}

} // namespace omvr::game::stereo
