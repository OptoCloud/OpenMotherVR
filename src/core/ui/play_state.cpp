// OpenMotherVR-Provenance: clean
#include "core/ui/play_state.h"

#include "game/constants.h"

#include <array>
#include <cstddef>

namespace omvr::core::ui {

namespace {

// The level folders of DATA\ENV\PRODUCTION (and its DLC folder) in the 2015
// Steam build. Names the game does not show anywhere are plain readings of
// the folder name.
constexpr std::array kLevels{
    Level{"FRONTEND", "Main menu"},
    Level{"BSP_TORRENS", "The Torrens"},
    Level{"BSP_LV426_PT01", "LV-426"},
    Level{"BSP_LV426_PT02", "LV-426"},
    Level{"TECH_RND", "Research and Development"},
    Level{"TECH_RND_HZDLAB", "Hazard Lab"},
    Level{"TECH_HUB", "Technology Hub"},
    Level{"TECH_COMMS", "Communications Tower"},
    Level{"TECH_MUTHRCORE", "APOLLO Core"},
    Level{"SCI_HUB", "Science Hub"},
    Level{"SCI_HOSPITALUPPER", "San Cristobal Medical"},
    Level{"SCI_HOSPITALLOWER", "San Cristobal Medical"},
    Level{"SCI_ANDROIDLAB", "Android Lab"},
    Level{"HAB_AIRPORT", "Spaceflight Terminal"},
    Level{"HAB_SHOPPINGCENTRE", "Shopping Centre"},
    Level{"HAB_CORPORATEPENT", "Corporate Penthouse"},
    Level{"ENG_REACTORCORE", "Reactor Core"},
    Level{"ENG_ALIEN_NEST", "The Nest"},
    Level{"ENG_TOWPLATFORM", "Tow Platform"},
    Level{"SOLACE", "Solace"},
    Level{"BSPNOSTROMO_RIPLEY", "Nostromo: Last Survivor"},
    Level{"BSPNOSTROMO_RIPLEY_PATCH", "Nostromo: Last Survivor"},
    Level{"BSPNOSTROMO_TWOTEAMS", "Nostromo: Crew Expendable"},
    Level{"BSPNOSTROMO_TWOTEAMS_PATCH", "Nostromo: Crew Expendable"},
    Level{"CHALLENGEMAP1", "Survivor Mode"},
    Level{"CHALLENGEMAP3", "Survivor Mode"},
    Level{"CHALLENGEMAP4", "Survivor Mode"},
    Level{"CHALLENGEMAP5", "Survivor Mode"},
    Level{"CHALLENGEMAP7", "Survivor Mode"},
    Level{"CHALLENGEMAP9", "Survivor Mode"},
    Level{"CHALLENGEMAP11", "Survivor Mode"},
    Level{"CHALLENGEMAP12", "Survivor Mode"},
    Level{"CHALLENGEMAP14", "Survivor Mode"},
    Level{"CHALLENGEMAP16", "Survivor Mode"},
    Level{"SALVAGEMODE1", "Salvage Mode"},
    Level{"SALVAGEMODE2", "Salvage Mode"},
};

// No leading separator: the game may open paths relative to DATA ("ENV\...").
constexpr std::string_view kProduction = "env\\production\\";
constexpr std::string_view kDlc        = "dlc\\";

char fold(char c) noexcept {
    if (c == '/') return '\\';
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool equal_folded(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (fold(a[i]) != fold(b[i])) return false;
    return true;
}

// Position of `needle` (lower case, backslashes) in `hay`, or npos.
std::size_t find_folded(std::string_view hay, std::string_view needle) noexcept {
    if (needle.size() > hay.size()) return std::string_view::npos;
    for (std::size_t i = 0; i + needle.size() <= hay.size(); ++i)
        if (equal_folded(hay.substr(i, needle.size()), needle)) return i;
    return std::string_view::npos;
}

} // namespace

std::string_view level_folder(std::string_view path) noexcept {
    const std::size_t at = find_folded(path, kProduction);
    if (at == std::string_view::npos) return {};
    std::string_view rest = path.substr(at + kProduction.size());
    if (rest.size() > kDlc.size() && equal_folded(rest.substr(0, kDlc.size()), kDlc))
        rest.remove_prefix(kDlc.size());
    const std::size_t end = rest.find_first_of("\\/");
    if (end == 0 || end == std::string_view::npos) return {}; // a file, not a folder
    return rest.substr(0, end);
}

std::span<const Level> levels() noexcept { return kLevels; }

int find_level(std::string_view folder) noexcept {
    for (std::size_t i = 0; i < kLevels.size(); ++i)
        if (equal_folded(kLevels[i].folder, folder)) return static_cast<int>(i);
    return -1;
}

void PlayState::on_file_open(std::string_view path) noexcept {
    const std::string_view folder = level_folder(path);
    if (folder.empty()) return;
    const int level = find_level(folder);
    if (level >= 0) level_.store(level, std::memory_order_relaxed);
}

void PlayState::on_event(std::string_view text) noexcept {
    if (text == game::kEventCutsceneStart)
        cutscene_.store(true, std::memory_order_relaxed);
    else if (text == game::kEventCutsceneFinish)
        cutscene_.store(false, std::memory_order_relaxed);
}

void PlayState::set_screen(Screen screen) noexcept {
    screen_.store(screen, std::memory_order_relaxed);
    // A load or the title screen ends any cutscene whose finish never came.
    if (screen == Screen::Loading || screen == Screen::Title)
        cutscene_.store(false, std::memory_order_relaxed);
}

} // namespace omvr::core::ui
