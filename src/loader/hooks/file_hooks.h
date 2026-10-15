// OpenMotherVR-Provenance: clean
#pragma once

namespace omvr::loader::hooks {

// Watches the files the game opens, for the level it loads (the Discord
// activity): AI.exe's import slots for CreateFileA and CreateFileW are
// pointed at pass-through wrappers that hand each path to
// context().play and call the real function. Only the game's own calls
// pass through them. Logs which slots were taken. Once, at attach.
void watch_game_file_opens(void* game_module) noexcept;

} // namespace omvr::loader::hooks
