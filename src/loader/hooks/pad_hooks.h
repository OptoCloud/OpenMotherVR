#pragma once

#include "game/sites/controls.h"

namespace omvr::loader::hooks {

// The replacements for the game pad's five methods and the left-stick
// prompt check.
[[nodiscard]] game::sites::PadReplacements pad_replacements() noexcept;

// For the stereo update, on the game's active pad device `pad` (04 §3.3).
// Game thread.
//
// A real pad button by pad code, through the stock IsDown: the recenter
// chord reads the real pad only, and the replaced IsDown answers false for
// raw codes. False before the stock method is known.
[[nodiscard]] bool stock_pad_button(void* pad, int pad_code) noexcept;
// The merged HasActivity (real pad or VR) when the pad slots are replaced,
// else the stock pad's own. False before the stock method is known.
[[nodiscard]] bool merged_pad_activity(void* pad) noexcept;
// The merged GetAxis of a virtual analog code (the player map's turn), 0
// when the pad slots are not replaced.
[[nodiscard]] float merged_pad_axis(void* pad, int virtual_code) noexcept;

} // namespace omvr::loader::hooks
