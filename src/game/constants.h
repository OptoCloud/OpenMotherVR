// OpenMotherVR-Provenance: clean
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// Facts about AI.exe other than addresses and layouts.
namespace omvr::game {

// Builds of AlienIsolation.exe (named "AI.exe" on disk) the mod can patch,
// by their PE IMAGE_FILE_HEADER.TimeDateStamp (01 §3.1).
enum class GameBuild : std::uint32_t {
    Original2015 = 0x54E3199D, // launch build
    Patched2020  = 0x5FD25676, // December 2020 Steam/Epic update
};

// The builds identify() knows, in the order they are tried.
inline constexpr std::array kKnownBuilds = {GameBuild::Original2015, GameBuild::Patched2020};

// The game's module name.
inline constexpr const char* kGameModuleName = "AI.exe";

// What the game's own pointers lead to before the UI set replaces them
// (01 §2.5.1).
inline constexpr std::string_view kStockUiPak = "data\\ui.pak";
inline constexpr std::string_view kShowBootup = "ShowBootup";

// Virtual control codes (04 §3): map 0 (player) 0x100.., map 1 (UI) 0x200..,
// map 2 0x400... Within a map, digital action k has code base + k and analog
// action j has base + num_digital + j. Real pad codes are 0x00..0x17 (A, B,
// X, Y, ...).
inline constexpr std::array<int, 3> kTierBase = {0x100, 0x200, 0x400};

// An action in the game's action maps holds at most this many bindings.
inline constexpr int kMaxBindings = 6;

// LibOVR's ovrRenderAPI_D3D11, the API tag of an EyeTexture.
inline constexpr std::int32_t kEyeTextureApiD3d11 = 5;

// ---- The stereo path (02) ---------------------------------------------------

// The size the game allocates for its stereo manager (02 §3.1); its
// allocation site pushes it.
inline constexpr std::uint32_t kGameStereoManagerSize = 0xB0;

// At the allocation site, the store of the new manager (`mov [reg+disp32],
// eax`) is this many bytes past the size push; the hand-off jumps to it.
inline constexpr std::uint32_t kStereoAllocStoreOffset = 0x24;

// Stereo device vtable slots the two recenter sites call (02 §6.4): the
// relative head pose, and the yaw-only reference pose they are pointed at.
inline constexpr std::uint32_t kDeviceHeadPoseSlot      = 11;
inline constexpr std::uint32_t kDeviceReferencePoseSlot = 21;

// The game's stereo mode numbers (05 §4.4): 0 renders the world, 1 the
// virtual screen.
inline constexpr std::int32_t kStereoModeWorld  = 0;
inline constexpr std::int32_t kStereoModeScreen = 1;

// ---- Menu screens and behaviours (05 §5, §6) --------------------------------

// Every menu screen class (front-end menu, pause menu, loading screen,
// attract screen) has Open in slot 0 and Close in slot 2.
inline constexpr std::uint32_t kScreenOpenSlot  = 0;
inline constexpr std::uint32_t kScreenCloseSlot = 2;

// FollowBehavior's activation pair.
inline constexpr std::uint32_t kFollowOnActivateSlot   = 0;
inline constexpr std::uint32_t kFollowOnDeactivateSlot = 1;

// FollowBehavior's "game look enabled" byte, which OnActivate sets (05 §7.6)
// with a `mov [reg+0x264], al` this many bytes into the method.
inline constexpr std::uint32_t kFollowLookEnabledOffset = 0x264;
inline constexpr std::uint32_t kFollowLookStoreOffset   = 8;

// ---- The gamepad (04 §3.3) --------------------------------------------------

// The XInput pad's vtable address in the table points at slot 1 (one past
// the destructor); the replaced methods' slots.
inline constexpr std::uint32_t kPadFirstTableSlot  = 1;
inline constexpr std::uint32_t kPadIsDownSlot      = 1;
inline constexpr std::uint32_t kPadGetValueSlot    = 2;
inline constexpr std::uint32_t kPadGetAxisSlot     = 3;
inline constexpr std::uint32_t kPadControlNameSlot = 5;
inline constexpr std::uint32_t kPadHasActivitySlot = 11;

// ---- Input (04 §2) ------------------------------------------------------------

// Keyboard scan codes the mod reads (04 §2.2).
inline constexpr int kScanLCtrl = 0x1D;
inline constexpr int kScanLAlt  = 0x38;
// The VR-off hotkey's right-hand keys (spec 22 section 13): DirectInput's
// right Ctrl, right Shift and F10.
inline constexpr int kScanRCtrl  = 0x9D;
inline constexpr int kScanRShift = 0x36;
inline constexpr int kScanF10    = 0x44;

// Pad control codes (04 §2.3). "No control" is -1, read as 16 bits 0xFFFF.
inline constexpr int kNoPadControl         = -1;
inline constexpr int kPadA                 = 0x00;
inline constexpr int kPadB                 = 0x01;
inline constexpr int kPadX                 = 0x02;
inline constexpr int kPadY                 = 0x03;
inline constexpr int kPadDpadUp            = 0x04;
inline constexpr int kPadDpadDown          = 0x05;
inline constexpr int kPadDpadLeft          = 0x06;
inline constexpr int kPadDpadRight         = 0x07;
inline constexpr int kPadBack              = 0x08;
inline constexpr int kPadStart             = 0x09;
inline constexpr int kPadLB                = 0x0A;
inline constexpr int kPadRB                = 0x0B;
inline constexpr int kPadRStick            = 0x0F;
inline constexpr int kPadLStickX           = 0x12;
inline constexpr std::size_t kPadCodeCount = 0x18; // the codes are 0x00..0x17

// Action maps (04 §2.4): 0 player, 1 menus, 2 a third map no VR action is
// routed to.
inline constexpr std::size_t kActionMapCount = 3;
inline constexpr int kPlayerMap              = 0;
inline constexpr int kMenuMap                = 1;

// Capacity of a map: digital and analog action entries, composite entries
// and the code dwords of one composite entry.
inline constexpr int kMaxDigitalActions = 0x29;
inline constexpr int kMaxAnalogActions  = 0x10;
inline constexpr int kMaxComposites     = 12;
inline constexpr int kCompositeCodes    = 7;

// Binding record device byte: the pad.
inline constexpr std::uint8_t kBindingDevicePad = 0;
// Binding record flag: the record refers to a composite entry, not a device
// control, so the map never passes its code to the pad.
inline constexpr std::uint8_t kBindingCompositeRef = 0x40;
// A composite code dword that delivers no control: -1 in the whole dword,
// which reads as "none" at 16 or 32 bits (04 §3.2 item 4).
inline constexpr std::uint32_t kNoCompositeCode = 0xFFFFFFFFu;

// Action indices VR drives (04 §2.4, §5.8.5). Indices 0x27 and 0x28 of the
// player map, 1, 9, 0x0B and 0x0E of the menu map are kept for parity; their
// game function is unconfirmed (04 §11).
namespace player_action {
inline constexpr int kSprint        = 0x08;
inline constexpr int kCrouch        = 0x09;
inline constexpr int kAim           = 0x0A;
inline constexpr int kFire          = 0x0B;
inline constexpr int kFire2         = 0x0E;
inline constexpr int kInteract      = 0x0C;
inline constexpr int kInteract2     = 0x0F;
inline constexpr int kRadial        = 0x0D;
inline constexpr int kReload        = 0x10;
inline constexpr int kReload2       = 0x11;
inline constexpr int kPrevWeapon    = 0x1A;
inline constexpr int kNextWeapon    = 0x1B;
inline constexpr int kTracker       = 0x1C;
inline constexpr int kFlashlight    = 0x1D;
inline constexpr int kMap           = 0x26;
inline constexpr int kLeftGripHeld  = 0x27;
inline constexpr int kRightGripHeld = 0x28;
// Analog.
inline constexpr int kMoveX = 0;
inline constexpr int kMoveY = 1;
inline constexpr int kTurn  = 3;
} // namespace player_action

namespace menu_action {
inline constexpr int kConfirm      = 0x00;
inline constexpr int kConfirm2     = 0x10;
inline constexpr int kRadial       = 0x01;
inline constexpr int kPause        = 0x08;
inline constexpr int kLeftGripHeld = 0x09;
inline constexpr int kBack         = 0x0A;
inline constexpr int kBack2        = 0x0F;
inline constexpr int kFlashlight   = 0x0B;
inline constexpr int kFlashlight2  = 0x0E;
// Analog.
inline constexpr int kTriggerL = 0;
inline constexpr int kTriggerR = 1;
inline constexpr int kMoveX    = 2;
inline constexpr int kMoveY    = 3;
inline constexpr int kTurnX    = 4;
inline constexpr int kTurnY    = 5;
} // namespace menu_action

// ---- Script event strings (05 §12, 11 §5.3) ----------------------------------

inline constexpr std::string_view kEventStartInteraction = "start_interaction";
inline constexpr std::string_view kEventStopInteraction  = "stop_interaction";
inline constexpr std::string_view kEventHidingStart      = "hiding_visibility_start";
inline constexpr std::string_view kEventHidingFinish     = "hiding_visibility_finish";
inline constexpr std::string_view kEventLadderFinish     = "ladder_interaction_finish";
inline constexpr std::string_view kEventSuitChangeFinish = "suit_change_interaction_finish";
inline constexpr std::string_view kEventCutsceneStart    = "cutscene_visibility_start";
inline constexpr std::string_view kEventCutsceneFinish   = "cutscene_visibility_finish";
// The hiding place's breathing game (spec 22 §2.3, §9.1).
inline constexpr std::string_view kEventBreathingGameStart = "Start_Breathing_Game";
inline constexpr std::string_view kEventBreathingGameEnd   = "End_Breathing_Game";
// "<prefix>_start" / "<prefix>_finish".
inline constexpr std::string_view kEventStartSuffix  = "_start";
inline constexpr std::string_view kEventFinishSuffix = "_finish";
inline constexpr std::string_view kEventSuitChange   = "suit_change_interaction";
inline constexpr std::string_view kEventKeypad       = "keypad_interaction";
inline constexpr std::string_view kEventHacking      = "hacking_interaction";
inline constexpr std::string_view kEventCutting      = "cutting_panel";
inline constexpr std::string_view kEventTerminal     = "terminal_interaction";
inline constexpr std::string_view kEventLadder       = "ladder_interaction";
inline constexpr std::string_view kEventRewire       = "rewire_interaction";
// Per-frame engine chatter starts with these; no tracked string does
// (11 §5.2).
inline constexpr char kEventChatterInitial            = 'C';
inline constexpr std::string_view kEventChatterPrefix = "on_";

// ---- Stereo (02 §4, §6.5; 05 §3.2g, §7.2; 08 §3.1, §4.1) -----------------------

// The scene visibility at StereoManager +0x64 (05 §3.2g): 1 shows the 3D
// scene, 0 blacks it out. HeadClipFade returns the same scale (08 §4.1).
inline constexpr float kSceneVisible = 1.0f;
inline constexpr float kSceneBlack   = 0.0f;

// Ids of the game's graphics settings the stereo path changes, and the
// stereo mode setting whose event starts the sensor (02 §6.5).
enum class GraphicsOption : std::int32_t {
    DepthOfField = 0x00,
    VSync        = 0x07,
    MotionBlur   = 0x0D,
    Ssao         = 0x15,
    StereoMode   = 0x18,
};

// The game's anti-aliasing modes, PostFX +0x1B3C (08 §3.1).
enum class AaMode : std::int32_t {
    Off     = 0,
    Fxaa    = 1,
    SmaaT1x = 2,
    SmaaT2x = 3,
};

// The game's head-clip threshold, 1e-5, bit for bit (05 §7.2).
inline constexpr std::uint32_t kHeadClipThresholdBits = 0x3727C5ACu;

// LibOVR 0.4 values (OVR_CAPI.h) for a DK2, the descriptor the game is
// shown (02 §4): ovrHmd_DK2; ovrHmdCap_Present | ovrHmdCap_Available |
// ovrHmdCap_LowPersistence; ovrTrackingCap_Orientation |
// ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position;
// ovrDistortionCap_TimeWarp.
inline constexpr std::int32_t kHmdTypeDk2         = 6;
inline constexpr std::uint32_t kDk2HmdCaps        = 0x83;
inline constexpr std::uint32_t kDk2SensorCaps     = 0x70;
inline constexpr std::uint32_t kDk2DistortionCaps = 0x02;

// ---- UI (05 §7.7) -----------------------------------------------------------

// In the game's display info, the UI reference size (1280x720) sits this
// far past the back-buffer size.
inline constexpr std::uint8_t kUiReferenceSizeOffset = 0x10;

// ---- Hands (21) --------------------------------------------------------------

// The game's weapon type enumeration (21 §2.5): the integer its
// `weapon_type` names parse to (0x4B5F80), held at weapon object +0xD4.
// Anything else is -1.
enum class WeaponType : std::int32_t {
    Unknown      = -1,
    NoWeapon     = 0,
    Flamethrower = 1,
    Pistol       = 2,
    Shotgun      = 3,
    Melee        = 4,
    Boltgun      = 5,
    Cattleprod   = 6,
};

// The per-bone flags of the game's pose (21 §2.3): the local transform is
// out of date (the model one is current), the model transform is out of
// date.
namespace pose_flag {
inline constexpr std::uint32_t kLocalStale = 1U << 0;
inline constexpr std::uint32_t kModelStale = 1U << 1;
} // namespace pose_flag

// The game's pad curve in RumbleSetMotors (21 §2.9), a level L (0 to 1) to
// a motor speed out of kMotorFullSpeed. At or below kDeadZone both motors
// stop. The low-frequency motor rises along one line up to kLowKnee and a
// steeper one above it; the high-frequency motor along one line.
namespace pad_rumble {
inline constexpr float kMotorFullSpeed = 65535.0f;
inline constexpr float kDeadZone       = 0.05f;
inline constexpr float kLowKnee        = 0.4f;
inline constexpr float kLowSlopeBelow  = 11468.626f;
inline constexpr float kLowOffsetBelow = 5242.8f;
inline constexpr float kLowSlopeAbove  = 75365.25f;
inline constexpr float kLowOffsetAbove = -9830.25f;
inline constexpr float kHighSlope      = 57343.0f;
inline constexpr float kHighOffset     = 8192.0f;
} // namespace pad_rumble

// 21 §2.2, §11.5 [likely]: how far the game lowers the first-person camera
// when the player crouches, game metres: the difference of the game's
// `first_person_camera_offset_y` (0.01) and
// `first_person_camera_crouch_offset_y` (-0.62) in DATA\GLOBALCONSTANTS.TXT.
inline constexpr float kCrouchCameraDropM = 0.63f;

} // namespace omvr::game
