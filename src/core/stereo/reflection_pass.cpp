// Planar reflections per eye. Analysis: dumps/07_reflections_head_pose.md.

#include "core/stereo/reflection_pass.h"

#include "core/config/settings_store.h"
#include "core/diag/logger.h"
#include "game/clear_gate.h"                     // R: stereo (A3): the skip byte through the thunk side
#include "game/stereo/follow_camera.h"           // R: stereo (A3): POV reads in the thunk
#include "game/stereo/renderer.h"                // R: stereo (A3): the context handed in
#include "game/stereo/stereo_objects.h"
#include "loader/hooks/guard.h"                  // R: stereo (A3): the capture guarded in the thunk
#include "platform/d3d11/render/texture_dump.h"  // R: stereo (A3): a debug capture port

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

namespace omvr::core::stereo {

namespace {

using game::DeviceState;
using game::StereoInterface;
using game::stereo::field;
using game::stereo::kPovAspect;
using game::stereo::kPovEye;
using game::stereo::kPovFlags;
using game::stereo::kPovFov;
using game::stereo::load_state;
using loader::hooks::guarded;

// Debug (F10): write what the reflection pass left bound as render target
// to <game dir>/mothervr_reflection_<pass>.dds.
void dump_bound_target(int pass) {
    if (!game::stereo::renderer()) return;
    platform::d3d11::dump_bound_target(
        game::stereo::renderer_get<ID3D11DeviceContext>(game::AiAddr::RendererContext), pass);
}

} // namespace

bool ReflectionPass::install(const game::stereo::ReflectionTargets& targets) noexcept {
    g_reflection = targets;
    g_reflection_ready = game::install_clear_gate(targets.clear);
    return g_reflection_ready;
}

void ReflectionPass::render(void* pov) noexcept {
    using RenderFn = void (__fastcall*)(void*, void*);
    auto render = reinterpret_cast<RenderFn>(g_reflection.render);
    auto* p = static_cast<std::uint8_t*>(pov);

    StereoInterface* dev = game::stereo::load_device(manager_);
    const DeviceState state = dev ? load_state(*dev) : DeviceState::None;
    {   // Diagnostics: each reflection POV and every change of its flags.
        static std::array<std::pair<const void*, int>, 8> s_seen{};
        static int s_lines = 0;
        auto it = std::find_if(s_seen.begin(), s_seen.end(),
                               [&](const auto& e) { return e.first == pov || e.first == nullptr; });
        if (it != s_seen.end() && (it->first == nullptr || it->second != p[kPovFlags]) && s_lines < 60) {
            ++s_lines;
            diag::logf("[stereo] reflection POV {} flags 0x{:02X} -> 0x{:02X} (state {}, size {}x{})",
                       pov, it->first ? it->second : -1, p[kPovFlags],
                       dev ? static_cast<int>(state) : -1,
                       field<std::int32_t>(p, 0x33C), field<std::int32_t>(p, 0x340));
            *it = {pov, p[kPovFlags]};
        }
    }
    if (!g_reflection_ready || !dev || state != DeviceState::Running ||
        (p[kPovFlags] & 0x11) != 0x11) {
        render(pov, nullptr);   // flat, or nothing to render: as the game does
        return;
    }

    // The centre (mono) state, restored for each eye and afterwards.
    std::array<std::uint8_t, 0xC0> centre;   // view, proj, proj copy
    std::memcpy(centre.data(), p, centre.size());
    const float saved_fov = field<float>(p, kPovFov);
    const float saved_aspect = field<float>(p, kPovAspect);
    const std::int32_t saved_eye = field<std::int32_t>(p, kPovEye);
    auto* owner = static_cast<std::uint8_t*>(*g_reflection.renderer_eye_owner);
    const std::int32_t saved_renderer_eye = owner ? field<std::int32_t>(owner, 0x2C) : 0;

    // The capture is a DebugOptions diagnostic: with it off the key does
    // nothing.
    static bool s_dump_key = false;
    const bool dump_key = config::current().debug.debug && keyboard_.key_down(ports::Key::F10);
    const bool dump = dump_key && !s_dump_key;
    s_dump_key = dump_key;

    for (int eye = 0; eye < 2; ++eye) {
        std::memcpy(p, centre.data(), 0x80);
        game::stereo::setup_eye(g_reflection, &manager_, p, eye);
        game::set_skip_clears(eye == 1);   // keep the first half
        render(pov, nullptr);
        game::set_skip_clears(false);
        if (dump) guarded("reflection capture", [&] { dump_bound_target(eye); });
    }

    std::memcpy(p, centre.data(), centre.size());
    field<float>(p, kPovFov) = saved_fov;
    field<float>(p, kPovAspect) = saved_aspect;
    field<std::int32_t>(p, kPovEye) = saved_eye;
    if (owner) field<std::int32_t>(owner, 0x2C) = saved_renderer_eye;

    static bool s_logged = false;
    if (!std::exchange(s_logged, true)) diag::log_line("[stereo] reflections rendered per eye");
}

} // namespace omvr::core::stereo
