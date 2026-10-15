// OpenMotherVR-Provenance: clean
#pragma once

#include <atomic>
#include <cstdint>
#include <span>
#include <string_view>

// What the player is doing, as far as the Discord activity needs it: the
// level the game last loaded, the menu screen that is up and whether a
// cutscene runs. Written by the game's hooks, read by the presence worker.
namespace omvr::core::ui {

// The full-screen menus the game opens over the world.
enum class Screen : std::uint8_t { None, Pause, Loading, Title };

// Each value is one atomic, so any thread reads it without tearing.
class PlayState final {
public:
    // A file the game opened. A level's files are under
    // ...\ENV\PRODUCTION\<level>\ or ...\ENV\PRODUCTION\DLC\<level>\, so the
    // first one names the level being loaded. Any thread; cheap for the
    // files that are not level files.
    void on_file_open(std::string_view path) noexcept;

    // A string the game's scripts passed to the event tap: cutscene start
    // and finish. Game thread.
    void on_event(std::string_view text) noexcept;

    // A menu screen opened (or Screen::None: it closed). Game thread.
    void set_screen(Screen screen) noexcept;

    // Index into levels() of the level last loaded, or -1 before the first.
    [[nodiscard]] int level() const noexcept { return level_.load(std::memory_order_relaxed); }
    [[nodiscard]] Screen screen() const noexcept { return screen_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool cutscene() const noexcept { return cutscene_.load(std::memory_order_relaxed); }

private:
    std::atomic<int> level_{-1};
    std::atomic<Screen> screen_{Screen::None};
    std::atomic<bool> cutscene_{false};
};

// The level folder in a path under ENV\PRODUCTION (case-insensitive, either
// slash), or empty when the path is not inside a level folder.
std::string_view level_folder(std::string_view path) noexcept;

struct Level {
    std::string_view folder; // as on disk, upper case
    std::string_view name;   // what the player is shown
};

// Every level of the 2015 game and its DLC; the front end is the main menu.
std::span<const Level> levels() noexcept;

// Index into levels() of `folder` (case-insensitive), or -1.
int find_level(std::string_view folder) noexcept;

} // namespace omvr::core::ui
