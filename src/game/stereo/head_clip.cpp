#include "game/stereo/head_clip.h"

namespace omvr::game::stereo {

namespace {

using HeadClipFadeFn = float (__stdcall*)(const float*, const float*);

// R: stereo (A3): a member of the object that owns the fix.
HeadClipFadeFn g_head_clip_fade = nullptr;

} // namespace

void install_head_clip_delay(void* head_clip_fade) noexcept {
    g_head_clip_fade = reinterpret_cast<HeadClipFadeFn>(head_clip_fade);
}

float call_head_clip_fade(const float* eye, const float* pivot) noexcept { return g_head_clip_fade(eye, pivot); }

} // namespace omvr::game::stereo
