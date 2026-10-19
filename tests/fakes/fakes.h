// OpenMotherVR-Provenance: clean
#pragma once

// Test doubles for the core ports: a clock the test moves, logs and sinks
// that record, in-memory files, and recording game-side ports.

#include "core/config/ports/file_store.h"
#include "core/diag/ports/clock.h"
#include "core/diag/ports/log.h"
#include "core/diag/ports/log_sink.h"
#include "core/input/ports/gesture_context.h"
#include "core/input/ports/stick_context.h"
#include "core/ui/ports/archive_files.h"
#include "core/ui/ports/game_signals.h"
#include "core/ui/ports/play_space_control.h"
#include "core/ui/ports/runtime_state.h"
#include "core/ui/ports/script_variables.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace omvr::tests {

// A clock that stands still until the test moves it.
class FakeClock final : public core::diag::ports::Clock {
public:
    std::chrono::steady_clock::time_point now() const noexcept override { return now_; }
    core::diag::ports::LocalTime local_time() const noexcept override { return local_; }

    void advance(std::chrono::milliseconds d) noexcept { now_ += d; }
    void set_local_time(const core::diag::ports::LocalTime& t) noexcept { local_ = t; }

private:
    std::chrono::steady_clock::time_point now_{std::chrono::seconds(10)};
    core::diag::ports::LocalTime local_{};
};

// Whether the left stick is free for the d-pad (a menu, an interaction), as
// the test sets it; by default it walks the player.
class SettableStick final : public core::input::ports::StickContext {
public:
    bool stick_free() const noexcept override { return free; }
    bool free = false;
};

// What the game is doing for the gestures, as the test sets it.
class FakeGestureContext final : public core::input::ports::GestureContext {
public:
    core::input::ports::GameContext game_context() const noexcept override { return state; }
    core::input::ports::GameContext state;
};

// A log that keeps every line.
class RecordingLog final : public core::diag::ports::Log {
public:
    void line(std::string_view msg) noexcept override { lines.emplace_back(msg); }
    void line_in(std::string_view msg, std::span<char>) noexcept override { lines.emplace_back(msg); }
    bool debug_enabled() const noexcept override { return debug; }

    bool contains(std::string_view text) const {
        for (const auto& l : lines) {
            if (l.find(text) != std::string::npos) return true;
        }
        return false;
    }

    std::vector<std::string> lines;
    bool debug = false;
};

// A sink that keeps every line it is handed.
class RecordingSink final : public core::diag::ports::LogSink {
public:
    bool ready() const noexcept override { return is_ready; }
    void write(std::span<const char> line) noexcept override {
        writes.emplace_back(line.begin(), line.end());
    }

    bool is_ready = true;
    std::vector<std::string> writes;
};

// ERROR_FILE_NOT_FOUND and ERROR_ACCESS_DENIED, as the platform adapters
// report them.
inline constexpr std::int32_t kFileNotFound = 2;
inline constexpr std::int32_t kAccessDenied = 5;

// The settings file in memory.
class MemoryFileStore final : public core::config::ports::FileStore {
public:
    util::Result<std::string> read(const std::filesystem::path& path) override {
        const auto it = files.find(path);
        if (it == files.end()) return std::unexpected(util::Error{kFileNotFound, "open"});
        return it->second;
    }
    util::Result<void> write_atomically(const std::filesystem::path& path, std::string_view text) override {
        files[path] = std::string(text);
        ++writes;
        if (events) events->push_back("write");
        return {};
    }
    util::Result<bool> exists(const std::filesystem::path& path) override { return files.contains(path); }

    std::map<std::filesystem::path, std::string> files;
    int writes                       = 0;
    std::vector<std::string>* events = nullptr;
};

// The game's exit and display-reset requests, recorded.
class FakeGameSignals final : public core::ui::ports::GameSignals {
public:
    void request_exit() noexcept override {
        ++exits;
        if (events) events->push_back("exit");
    }
    void request_display_reset() noexcept override { ++resets; }
    bool can_reset_display() const noexcept override { return can_reset; }

    int exits                        = 0;
    int resets                       = 0;
    bool can_reset                   = true;
    std::vector<std::string>* events = nullptr;
};

// This run's runtime as the test sets it.
class FakeRuntimeState final : public core::ui::ports::RuntimeState {
public:
    std::optional<core::config::VRRuntime> forced() const noexcept override { return forced_runtime; }
    bool failed() const noexcept override { return has_failed; }
    core::vr::DetectResult::Chosen chosen() noexcept override { return chosen_runtime; }
    bool session_active() const noexcept override { return active; }

    std::optional<core::config::VRRuntime> forced_runtime;
    bool has_failed                               = false;
    core::vr::DetectResult::Chosen chosen_runtime = core::vr::DetectResult::Chosen::OpenVR;
    bool active                                   = true;
};

// The calibration and the -vrscale switch as the test sets them; counts
// calibration requests.
class FakePlaySpaceControl final : public core::ui::ports::PlaySpaceControl {
public:
    void request_height_calibration() noexcept override { ++requests; }
    core::ui::ports::CalibrationView height_calibration() noexcept override { return calibration; }
    std::optional<float> world_scale_switch() const noexcept override { return scale_switch; }

    int requests = 0;
    core::ui::ports::CalibrationView calibration{};
    std::optional<float> scale_switch;
};

// Every script-variable call, as "name=value" in call order.
class RecordingScriptVariables final : public core::ui::ports::ScriptVariables {
public:
    bool ready() const noexcept override { return is_ready; }
    void set_bool(core::ui::ports::UiHandle, const char* name, bool value) noexcept override {
        calls.push_back(std::string(name) + "=" + (value ? "true" : "false"));
    }
    void set_int(core::ui::ports::UiHandle, const char* name, int value) noexcept override {
        calls.push_back(std::string(name) + "=" + std::to_string(value));
    }
    void set_float(core::ui::ports::UiHandle, const char* name, float value) noexcept override {
        calls.push_back(std::string(name) + "=" + std::to_string(value));
    }
    bool add_row(core::ui::ports::UiHandle, const char* name, const char* a,
                 const char* b) noexcept override {
        calls.push_back(std::string(name) + "=" + a + "|" + b);
        return ++rows < row_limit;
    }

    bool is_ready = true;
    int rows      = 0;
    int row_limit = 1000;
    std::vector<std::string> calls;
};

// The archive files in memory.
class FakeArchiveFiles final : public core::ui::ports::ArchiveFiles {
public:
    using Bytes = std::vector<std::byte>;

    std::optional<core::ui::ports::FileIdentity> identity(
        const std::filesystem::path& path) noexcept override {
        const auto it = files.find(path);
        if (it == files.end()) return std::nullopt;
        return core::ui::ports::FileIdentity{it->second.size(), 0};
    }
    util::Result<Bytes> read(const std::filesystem::path& path) override {
        const auto it = files.find(path);
        if (it == files.end()) return std::unexpected(util::Error{kFileNotFound, "open"});
        return it->second;
    }
    util::Result<void> write_atomically(const std::filesystem::path& path,
                                        std::span<const std::byte> data) override {
        if (fail_create) return std::unexpected(util::Error{kAccessDenied, "create"});
        files[path] = Bytes(data.begin(), data.end());
        return {};
    }
    bool stamped(const std::filesystem::path&, core::ui::ports::FileTime) noexcept override { return false; }
    void stamp(const std::filesystem::path&, core::ui::ports::FileTime) noexcept override {}
    std::optional<core::ui::ports::FileIdentity> read_sidecar(
        const std::filesystem::path&) noexcept override {
        return std::nullopt;
    }
    void write_sidecar(const std::filesystem::path&, const core::ui::ports::FileIdentity&) noexcept override {
    }
    void remove(const std::filesystem::path& path) noexcept override { files.erase(path); }
    core::ui::ports::LockState lock(std::wstring_view, std::chrono::milliseconds) noexcept override {
        return {true, false};
    }
    void unlock() noexcept override {}
    std::span<const std::byte> ui_patch() noexcept override { return {}; }

    std::map<std::filesystem::path, Bytes> files;
    bool fail_create = false;
};

// Bytes of a string, for building binary inputs.
inline std::vector<std::byte> bytes_of(std::string_view s) {
    std::vector<std::byte> out(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) out[i] = static_cast<std::byte>(s[i]);
    return out;
}

// A byte range as a string, for comparing.
inline std::string string_of(std::span<const std::byte> b) {
    std::string out(b.size(), '\0');
    for (std::size_t i = 0; i < b.size(); ++i) out[i] = static_cast<char>(b[i]);
    return out;
}

} // namespace omvr::tests
