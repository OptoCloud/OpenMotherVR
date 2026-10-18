// OpenMotherVR-Provenance: clean
#include "loader/context.h"

#include "core/ui/ui_contract.h"
#include "loader/hooks/haptic_hooks.h"
#include "loader/resource_ids.h"
#include "platform/openvr/openvr_runtime.h"
#include "platform/openxr/openxr_runtime.h"
#include "platform/win32/process_info.h"

#include <memory>

namespace omvr::loader {

namespace {

// Set once, in DLL_PROCESS_ATTACH, before any hook can run: the loader lock
// orders it before every thread that calls context().
Context* the_context = nullptr;

// The runtime the stereo device drives, made when the game creates its
// stereo device.
// Spec 21 §16: the hands' aim pose actions are always declared, bound and
// read, so Hands.Enabled turned on in the menu has them at once (21 §15.1);
// the hands use them only while it is on. 21 §14.3: the haptic output only
// when Hands.Haptics.Enabled was on at attach and its game sources went in.
std::unique_ptr<core::vr::ports::VrRuntime> make_runtime(Context& c, core::vr::DetectResult::Chosen chosen) {
    constexpr bool aim_poses = true;
    core::vr::ports::HapticSource* haptics =
        c.settings.current().hands.haptics.enabled && hooks::haptics_installed() ? &c.haptics : nullptr;
    if (chosen == core::vr::DetectResult::Chosen::OpenVR) {
        return platform::openvr::make_openvr_runtime({c.log, c.clock, c.failure, c.archive_files, c.body_controls,
                                                      c.dialogs, c.runtimes.last_result().openvr_runtime_path,
                                                      aim_poses, haptics});
    }
    return platform::openxr::make_openxr_runtime(
        {c.log, c.xr, c.body_controls, c.headset_audio, c.console, aim_poses, haptics});
}

} // namespace

Context::Context(void* self) noexcept
    : module(self),
      flush_schedule(thread_roles),
      log_flusher(log_sink, clock, flush_schedule, log),
      log_out(log_sink, log_flusher, current_thread, thread_roles),
      log(clock, log_out, platform::win32::current_process_id()),
      crash(log),
      settings(files, log),
      dialogs(core::ui::kCaption, log),
      archive_files(self, OMVR_IDR_OPENVR_API, OMVR_IDR_UI_PATCH, log),
      ui_archive(archive_files, dialogs, log),
      runtime_state(runtimes, stereo, launch),
      play_space_control(calibration, clock, launch),
      display_reset(console, runtime_state, log),
      schemes(glyphs, settings.current().controls, log),
      pad(glyphs, log),
      stick_context(manager, interactions),
      gesture_context(manager, interactions),
      gestures({settings.current().controls, gesture_context, log}),
      controls({pad, schemes, settings.current().input, settings.current().controls, stick_context, clock,
                log, &gestures}),
      interactions(log),
      hands(settings.current().hands, log),
      haptics(settings.current().hands.haptics, clock, log),
      hud_toggle({controls, settings.current().controls, hud_visibility, clock, log}),
      body_hider(log),
      body_controls(hud_toggle, clock),
      settings_push(
          {script_vars, display_reset, schemes, settings.current(), runtime_state, play_space_control, log}),
      commands(settings, display_reset, console, play_space_control, log),
      xr(log, clock, dialogs, failure),
      openvr_probe(log),
      openxr_probe(xr),
      runtimes(log, settings.current(), launch, openvr_probe, openxr_probe),
      virtual_screen(xr),
      headset_audio(dialogs, log, addresses),
      modes(log),
      update(settings.current(), scene),
      stereo(manager, update, dialogs, settings.current(), launch, runtimes, failure, log),
      head_clip(clock),
      play_space(log),
      calibration(log),
      stereo_aa(settings.current(), log),
      reflections(keyboard, settings.current(), log),
      screen_policy(virtual_screen, keyboard, stereo, runtimes, log),
      mirror(platform::d3d11::MirrorObjects{manager.si.mirror_vs, manager.si.mirror_ps,
                                            manager.si.mirror_cb_screen, manager.si.mirror_cb,
                                            manager.si.mirror_sampler},
             &manager.si.eye_texture[0].srv, log),
      vignette(log),
      window(stereo, log),
      shader_fixes(log) {}

Context& create_context(void* module) noexcept {
    // Leaked on purpose: nothing of the mod runs at process exit (no
    // destructors under the loader lock).
    static Context& ctx = *new Context(module);
    the_context         = &ctx;

    // The debug setting the logger reports may change while the game runs.
    ctx.log.set_debug_flag(&ctx.settings.current().debug.debug);

    ctx.stereo.set_runtime_factory(
        [c = &ctx](core::vr::DetectResult::Chosen chosen) { return make_runtime(*c, chosen); });
    return ctx;
}

Context& context() noexcept { return *the_context; }

} // namespace omvr::loader
