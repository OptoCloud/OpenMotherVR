#pragma once

#include "core/diag/ports/log.h"
#include "core/ui/ports/archive_files.h"
#include "platform/win32/named_mutex.h"

namespace omvr::platform::win32 {

// The payloads embedded in the DLL's resources and the files they are
// deployed to in the game directory.
//
// The original writes through a file mapping and memcpy; a plain WriteFile is
// equivalent. Both compare the target's last-write time against the mod's own
// PE build stamp and skip the write when they match; MOTHER.PAK must also
// still match the UI.PAK it was built from (a sidecar file records which).
// Game thread at attach; deploy_openvr_api on the thread that starts OpenVR.
class ArchiveFiles final : public core::ui::ports::ArchiveFiles {
public:
    // `module` is this DLL; the ids are its BINARY resources.
    ArchiveFiles(void* module, int openvr_api_id, int ui_patch_id, core::diag::ports::Log& log) noexcept
        : module_(module), openvr_api_id_(openvr_api_id), ui_patch_id_(ui_patch_id), log_(log) {}

    std::optional<core::ui::ports::FileIdentity> identity(
        const std::filesystem::path& path) noexcept override;
    util::Result<std::vector<std::byte>> read(const std::filesystem::path& path) override;
    util::Result<void> write_atomically(const std::filesystem::path& path,
                                        std::span<const std::byte> data) override;
    bool stamped(const std::filesystem::path& path, core::ui::ports::FileTime stamp) noexcept override;
    void stamp(const std::filesystem::path& path, core::ui::ports::FileTime stamp) noexcept override;
    std::optional<core::ui::ports::FileIdentity> read_sidecar(
        const std::filesystem::path& path) noexcept override;
    void write_sidecar(const std::filesystem::path& path,
                       const core::ui::ports::FileIdentity& id) noexcept override;
    void remove(const std::filesystem::path& path) noexcept override;
    core::ui::ports::LockState lock(std::wstring_view name,
                                    std::chrono::milliseconds timeout) noexcept override;
    void unlock() noexcept override;
    std::span<const std::byte> ui_patch() noexcept override;

    // Same scheme for the embedded openvr_api.dll 1.0.10, written next to
    // this DLL. The original does this in DllMain when the configured runtime
    // is OpenVR; here the OpenVR runtime calls it just before it loads the
    // DLL, so the write is outside the loader lock, and it also happens when
    // OpenVR is reached by fallback or chosen in the menu after startup.
    //
    // From: FUN_1001e940 @ 0x1001e940.
    bool deploy_openvr_api() noexcept;

private:
    // Writes resource `id` to `target` and stamps it with `build_stamp`.
    bool write_resource(int id, const std::filesystem::path& target,
                        core::ui::ports::FileTime build_stamp) noexcept;

    void* module_;
    int openvr_api_id_;
    int ui_patch_id_;
    core::diag::ports::Log& log_;
    NamedMutex mutex_;
};

} // namespace omvr::platform::win32
