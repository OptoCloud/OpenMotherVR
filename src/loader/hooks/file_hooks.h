// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"

#include <filesystem>

namespace omvr::loader::hooks {

// The engine settings override (24 §6): with Graphics.EngineSettingsOverride
// and at least one addition on, and the game's load of its engine settings
// file found as expected, arms the redirect of that file's open: at the
// game's first open (display initialisation, main thread) a copy of the
// player's file with the additions is built in the game directory, and the
// game opens the copy instead. True when armed; watch_game_file_opens must
// then take the slots. Logged either way. Once, at attach.
bool prepare_engine_settings(const game::GameImage& g, const std::filesystem::path& game_dir) noexcept;

// Watches the files the game opens: AI.exe's import slots for CreateFileA
// and CreateFileW are pointed at wrappers that hand each path to
// context().play when `levels` (the level it loads, for the Discord
// activity), redirect the engine settings file when prepare_engine_settings
// armed it, and call the real function. Only the game's own calls pass
// through them. Logs which slots were taken. Once, at attach.
void watch_game_file_opens(void* game_module, bool levels) noexcept;

} // namespace omvr::loader::hooks
