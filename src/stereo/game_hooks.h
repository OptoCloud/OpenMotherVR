#pragma once

#include <cstdint>

// The game-side hooks around the stereo device: the per-frame stereo update
// (snap turn, recenter, blackout), placement of the menu screen and the HUD
// panel, the stereo mode stack, the menu wrappers that switch to the
// virtual screen, and the trackers they rely on (active first-person camera,
// cutscenes and fades).
//
// Every function here replaces a game function at a call site or vtable
// slot and keeps that function's ABI (noted per function). The hook
// installer does the patching.
//
// From: FUN_1001f8a0, FUN_1001fc20, FUN_1001ffd0, FUN_100201e0,
// FUN_10020220, FUN_10020c80..FUN_10020dd0, FUN_10001720, FUN_10001740,
// FUN_1001f830, FUN_10003100. Analysis: dumps/06c_detours_and_patches.md.

namespace mothervr::stereo {

struct StereoManager;

// AI.exe functions these hooks call or wrap. Set by install().
struct GameHookTargets {
    void* follow_on_activate;    // FollowBehavior vtable slot 0 (original)
    void* follow_on_deactivate;  // slot 1
    void* ui_on_message;         // UI entity vtable slot 2 (original)
    void* menu_close;            // front-end menu slot 2
    void* pause_open;            // pause menu slot 0
    void* pause_close;           // pause menu slot 2
    void* loading_open;          // loading screen slot 0
    void* loading_close;         // loading screen slot 2
    void* attract_open;          // attract/title screen slot 0
};
void install_game_hooks(const GameHookTargets& targets) noexcept;

// The game's mode-stack append (spec 05 section 3.2f): thiscall on the
// stack array at StereoManager +0x68, one `const int*` argument, ret 4;
// grows the array through the game's allocator, raises the count and
// returns the new element, or null when the allocation failed. Set when
// the installer found it at its call in StereoPushMode; without it the
// count is raised only into storage the array already has.
void install_mode_stack_append(void* append) noexcept;

// The "head in wall" blackout threshold the game compares against (its
// address is patched into the game). 1e-5 with clipping blinders on, 1.0 off.
float& head_clip_threshold() noexcept;
void   set_clipping_blinders(bool on) noexcept;

// Recenter on the next stereo update: now, or once the headset is in use.
// Used when gameplay starts (a loading screen closes) and at launch.
void request_recenter() noexcept;

// ---- Call-site replacements ------------------------------------------------

// Stereo::Update(float dt). __thiscall, ret 4.
int __fastcall stereo_update(StereoManager* self, void* edx, float dt) noexcept;

// Matrix44* BuildUiQuad(Matrix44* out, float size, float inv_aspect, float dist).
// __cdecl; the caller copies the result through the returned pointer.
float* __cdecl place_virtual_screen(float* out, float size, float inv_aspect, float dist) noexcept;
float* __cdecl place_hud_panel(float* out, float size, float inv_aspect, float dist) noexcept;

// Stereo::PushMode(int) (__thiscall, ret 4) and Stereo::PopMode() (ret 0).
void __fastcall push_mode(StereoManager* self, void* edx, int mode) noexcept;
void __fastcall pop_mode(StereoManager* self, void* edx) noexcept;

// IsMoveOnLeftStick() for the control prompts. __thiscall, no args.
bool __fastcall is_move_on_left_stick(void* self, void* edx) noexcept;

// ---- Planar reflections (not in the original) --------------------------------
//
// The game renders the floor reflection once, mono, with the flat camera's
// projection, and the floor shader samples it in screen space over the
// whole side-by-side target, so in VR it slides against head motion. With
// stereo running, render it per eye into the two halves of its target
// instead: the reflection view is mirrored in X, so half k rendered with
// eye k's transform is what the other eye samples. Each render clears the
// whole target, so the second pass runs with clears skipped.
// Analysis: dumps/07_reflections_head_pose.md.
struct ReflectionTargets {
    void*  render;               // ReflectionPOV render
    void*  apply_eye_transform;  // Stereo::ApplyEyeTransform
    void*  update_frustum;       // POV frustum update
    void** renderer_eye_owner;   // its +0x2C is the renderer's current eye
    std::uint8_t* clear;         // the game's clear of the bound target
};
// False if the clear could not be gated (its code differs from the
// expected build); the fix then stays off.
bool install_reflection_fix(const ReflectionTargets& targets) noexcept;

// Replaces the call of ReflectionPOV render. __thiscall, no args.
void __fastcall render_reflection(void* pov, void* edx) noexcept;

// ---- Anti-aliasing in stereo (not in the original) ----------------------------
//
// In stereo the game's final post chain takes its own output path: it binds
// postprocess_stereo_final, sets the head-in-wall fade in PS c36 and draws
// straight into the output, never calling the AA pass. Mono runs the AA
// pass instead, which draws the bound final effect into an intermediate
// target and then runs FXAA or SMAA from it into the output. With stereo
// running and AA on, the stereo output draw is replaced by that AA pass,
// so the stereo final effect (fade included) feeds FXAA/SMAA exactly as the
// mono effect does.
//
// FXAA and SMAA 1x are spatial: they act on the side-by-side image, and the
// only pixels they can mix across the eye seam are the column on each side
// of it, at the outer edge of each eye's view. SMAA T2x is temporal: it
// needs per-eye velocities and per-eye previous view-projections, which the
// game keeps for one eye only (the velocity pass runs for eye 0), so its
// history would reproject the right eye with the left eye's motion. In
// stereo T2x therefore runs as SMAA 1x, without the temporal resolve and
// without projection jitter.
struct StereoAaTargets {
    void* fullscreen_draw;  // full-screen triangle draw (cdecl, no args)
    void* postfx;           // PostFX instance getter (cdecl, no args)
    void* aa_pass;          // PostFX AA pass (thiscall, no args)
    void* smaa;             // SMAA runner (cdecl; color, second input, const bool* temporal)
};
void install_stereo_aa(const StereoAaTargets& targets) noexcept;

// Replaces the full-screen draw of the stereo output path. __cdecl, no args.
void __cdecl stereo_final_draw() noexcept;

// Replaces the AA pass's call of the SMAA runner. __cdecl.
void __cdecl run_smaa(void* color, void* second, const std::uint8_t* temporal) noexcept;

// ---- Object culling (not in the original) ----------------------------------
//
// The game's per-object visibility loop (0xa1fd20, run on job threads)
// tests each object against a distance/LOD limit, the view frustum, and,
// where its cull context has them, two optional tests. Which POV culls
// decides which of those exist (POV +0x314 is its render-list group,
// CreatePOV's second argument):
//   group 2, BUILD_MAIN_RENDER_LIST: the main POV, the only one whose +0x310
//     holds the software occlusion buffer (ctx+0x5c), drawn from the single
//     centre view. In stereo it drops objects one eye still sees.
//   group 1, BUILD_SHADOWS_RENDER_LISTS: the shadow lights (POV type 2),
//     the only ones that get a view volume (ctx+0x60); the game's test runs.
//   group 0, BUILD_PRE_MAIN_RENDER_LISTS: the reflection POV, neither test.
// So the occlusion replacement below only ever sees main-POV culling, and
// no phase global is needed. The distance limit (scaled from the FOV)
// makes small props pop in and out, in every group.
struct CullTargets {
    void* distance;     // bool __cdecl (const bounds*, const vec3* camera, float scale)
    void* occlusion;    // bool __cdecl (const bounds*, const ctx*, const buffer*)
};
void install_cull_fix(const CullTargets& targets) noexcept;

// Replace two calls in the loop. __cdecl, result in al. While the stereo
// device runs: the distance test passes in every group, the occlusion test
// passes for the main POV.
bool __cdecl cull_distance(const void* bounds, const float* camera, float scale) noexcept;
bool __cdecl cull_occlusion(const void* bounds, const void* ctx, const void* buffer) noexcept;

// ---- Head-in-wall blackout delay (not in the original) ----------------------
//
// HeadClipFade (VRCameraBehavior) returns the scene visibility: below 1 when
// the head is away from the body or inside geometry. Walking through a door
// clips the head for a moment and flashes the view black. In stereo, a fade
// that starts is held at 1 for 0.6 s and only then let through.
// Analysis: dumps/06c_detours_and_patches.md bdc+3.
void install_head_clip_delay(void* head_clip_fade) noexcept;

// float HeadClipFade(const vec3* eye, const vec3* pivot). __stdcall, ret 8,
// result in ST0.
float __stdcall head_clip_fade(const float* eye, const float* pivot) noexcept;

// ---- Vtable replacements ---------------------------------------------------

// FollowBehavior OnActivate / OnDeactivate(arg). __thiscall, ret 4.
int __fastcall follow_on_activate(void* self, void* edx, void* arg) noexcept;
int __fastcall follow_on_deactivate(void* self, void* edx, void* arg) noexcept;

// UI entity OnMessage(const ShortGuid*, a, b, c). __thiscall, ret 0x10.
int __fastcall ui_on_message(void* self, void* edx, const std::uint32_t* msg, int a, int b, int c) noexcept;

// Menu Open / Close. __thiscall, no args.
void __fastcall menu_close(void* self, void* edx) noexcept;
void __fastcall pause_open(void* self, void* edx) noexcept;
void __fastcall pause_close(void* self, void* edx) noexcept;
void __fastcall loading_open(void* self, void* edx) noexcept;
void __fastcall loading_close(void* self, void* edx) noexcept;
void __fastcall attract_open(void* self, void* edx) noexcept;
void __fastcall attract_close(void* self, void* edx) noexcept;

} // namespace mothervr::stereo
