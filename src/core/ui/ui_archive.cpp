// The original writes through a file mapping and memcpy; a plain write is
// equivalent. It compares the target's last-write time against the mod's own
// PE build stamp and skips the write when they match; MOTHER.PAK must also
// still match the UI.PAK it was built from (a sidecar file records which).

#include "core/ui/ui_archive.h"

#include "core/diag/log_format.h"
#include "core/ui/compat_messages.h"
#include "core/ui/tuning.h"
#include "core/ui/ui_contract.h"
#include "core/ui/ui_patch.h"

namespace omvr::core::ui {

namespace {

using diag::path_text;
using diag::ports::logf;

// The three files of a deploy.
struct ArchivePaths {
    std::filesystem::path stock;   // DATA\UI.PAK, the player's own
    std::filesystem::path target;  // DATA\MOTHER.PAK
    std::filesystem::path sidecar; // DATA\MOTHER.PAK.src
};

ArchivePaths archive_paths(const std::filesystem::path& game_dir) {
    const auto data = game_dir / kDataDirectory;
    ArchivePaths p{data / kStockArchiveName, data / kModArchiveName, {}};
    p.sidecar = p.target;
    p.sidecar += kSidecarSuffix;
    return p;
}

} // namespace

bool UiArchiveDeployer::reuse_or_build(const std::filesystem::path& stock, const std::filesystem::path& target,
                                       const std::filesystem::path& sidecar, const ports::FileIdentity& source,
                                       std::int64_t build_stamp, bool may_build) noexcept {
    if (files_.stamped(target, build_stamp) && files_.read_sidecar(sidecar) == source) {
        logf(log_, "payload: {} is current", path_text(target));
        return true;
    }
    if (!may_build) {
        logf(log_,
             "payload: {} is being built by another process and is not ready after 5 s; "
             "options page off this session, VR not affected",
             path_text(target));
        return false;
    }

    // Built from the player's own UI.PAK and the embedded patch; no game
    // files ship with the mod.
    files_.remove(sidecar);
    if (!build_ui_archive(files_, log_, stock, files_.payload(ports::ResourceId::UiPatch), target)) return false;
    files_.stamp(target, build_stamp);
    files_.write_sidecar(sidecar, source);
    return true;
}

bool UiArchiveDeployer::deploy(const std::filesystem::path& game_dir, std::int64_t build_stamp) noexcept {
    const ArchivePaths paths = archive_paths(game_dir);

    // MOTHER.PAK is built from UI.PAK, so a game update or a language switch
    // that replaces UI.PAK makes it stale even when the DLL is unchanged.
    const auto source = files_.identity(paths.stock);
    if (!source) {
        logf(log_, "payload: {} not found", path_text(paths.stock));
        // The original disables the mod here; we only lose the options page.
        dialogs_.post(ui_pak_missing(path_text(paths.stock)), ports::DialogKind::Error, nullptr, "UI.PAK missing");
        return false;
    }
    // When Steam relaunches the game, two processes attach close together
    // and both come here: one builds, the other waits for it and then
    // reuses its archive, instead of both writing the same files.
    const ports::LockState lock = files_.lock(kArchiveLockName, kArchiveLockWait);
    // Without a mutex at all (cannot be created) build as before.
    const bool may_build = lock.held || !lock.exists;
    const bool ok = reuse_or_build(paths.stock, paths.target, paths.sidecar, *source, build_stamp, may_build);
    files_.unlock();
    return ok;
}

} // namespace omvr::core::ui
