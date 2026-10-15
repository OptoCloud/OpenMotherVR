#pragma once

#include "core/diag/ports/log.h"
#include "core/ui/ports/archive_files.h"
#include "core/ui/ports/dialogs.h"

#include <cstdint>
#include <filesystem>

namespace omvr::core::ui {

// The mod's UI archive, DATA\MOTHER.PAK. Game thread, at attach.
class UiArchiveDeployer {
public:
    UiArchiveDeployer(ports::ArchiveFiles& files, ports::Dialogs& dialogs, diag::ports::Log& log) noexcept
        : files_(files), dialogs_(dialogs), log_(log) {}

    // Build the mod's UI archive at <game_dir>\DATA\MOTHER.PAK from the stock
    // DATA\UI.PAK and the embedded patch (see ui_patch.h), unless a copy
    // stamped with `build_stamp` (a FILETIME, the DLL's build time) and built
    // from the UI.PAK now there is already in place. The stamp is applied to
    // all three file times so a mod update replaces the file exactly once;
    // DATA\MOTHER.PAK.src records UI.PAK's size and last-write time, so a
    // game update or a language switch rebuilds it too.
    //
    // Returns false if DATA\UI.PAK is missing (a message box is posted) or is
    // not the version the patch was made for, or the write failed; the caller
    // must then leave the game's UI archive alone (the options page is
    // missing, VR still works).
    //
    // From: FUN_1001e720 @ 0x1001e720.
    bool deploy(const std::filesystem::path& game_dir, std::int64_t build_stamp) noexcept;

private:
    // Reuses MOTHER.PAK if it is current, else builds it (unless `may_build`
    // is false: another process holds the build lock).
    bool reuse_or_build(const std::filesystem::path& stock, const std::filesystem::path& target,
                        const std::filesystem::path& sidecar, const ports::FileIdentity& source,
                        std::int64_t build_stamp, bool may_build) noexcept;

    ports::ArchiveFiles& files_;
    ports::Dialogs& dialogs_;
    diag::ports::Log& log_;
};

} // namespace omvr::core::ui
