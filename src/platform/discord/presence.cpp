// OpenMotherVR-Provenance: clean
#include "platform/discord/presence.h"

#include "platform/discord/constants.h"
#include "platform/win32/unique_handle.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <format>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>
#include <windows.h>

namespace omvr::platform::discord {

namespace {

using core::diag::ports::logf;
using nlohmann::json;

struct Frame {
    std::uint32_t op = 0;
    std::string payload;
};

// One connection to the Discord client's IPC pipe. Synchronous: every write
// is followed by reading Discord's reply, so the worker is the only reader.
class Connection {
public:
    // Opens the first pipe Discord listens on. False: Discord not running.
    bool open() noexcept {
        for (int i = 0; i < kPipeCount; ++i) {
            const std::wstring name = std::format(L"\\\\.\\pipe\\discord-ipc-{}", i);
            HANDLE h = ::CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                                     nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                pipe_.reset(h);
                return true;
            }
        }
        return false;
    }

    void close() noexcept { pipe_.reset(); }
    bool is_open() const noexcept { return pipe_.valid(); }

    bool write(std::uint32_t op, std::string_view payload) noexcept {
        std::vector<char> buf(8 + payload.size());
        const auto len = static_cast<std::uint32_t>(payload.size());
        std::memcpy(buf.data(), &op, 4);
        std::memcpy(buf.data() + 4, &len, 4);
        std::memcpy(buf.data() + 8, payload.data(), payload.size());
        return write_all(buf.data(), buf.size());
    }

    std::optional<Frame> read() noexcept {
        std::uint32_t header[2]{};
        if (!read_all(header, sizeof(header)) || header[1] > kMaxFrame) return std::nullopt;
        Frame f{header[0], std::string(header[1], '\0')};
        if (header[1] != 0 && !read_all(f.payload.data(), header[1])) return std::nullopt;
        return f;
    }

private:
    bool write_all(const void* data, std::size_t size) noexcept {
        DWORD done = 0;
        return ::WriteFile(pipe_.get(), data, static_cast<DWORD>(size), &done, nullptr) && done == size;
    }

    bool read_all(void* data, std::size_t size) noexcept {
        auto* p = static_cast<char*>(data);
        while (size != 0) {
            DWORD got = 0;
            if (!::ReadFile(pipe_.get(), p, static_cast<DWORD>(size), &got, nullptr) || got == 0)
                return false;
            p += got;
            size -= got;
        }
        return true;
    }

    win32::UniqueHandle pipe_;
};

std::string handshake(std::string_view application_id) {
    return json{{"v", kProtocolVersion}, {"client_id", application_id}}.dump();
}

std::string set_activity(const Activity& a, unsigned nonce) {
    json activity = json::object();
    if (!a.details.empty()) activity["details"] = a.details;
    if (!a.state.empty()) activity["state"] = a.state;
    if (a.start_unix != 0) activity["timestamps"] = {{"start", a.start_unix}};
    activity["assets"] = {{"large_image", kLargeImage}, {"large_text", kLargeText}};
    return json{{"cmd", "SET_ACTIVITY"},
                {"args", {{"pid", ::GetCurrentProcessId()}, {"activity", activity}}},
                {"nonce", std::to_string(nonce)}}
        .dump();
}

// Reads frames until the reply to our last write: answers pings, and gives
// up on a close or a broken pipe. The reply's "evt" is "ERROR" when Discord
// refused the command; its message is returned in `error`.
bool await_reply(Connection& c, std::string& error) noexcept {
    for (;;) {
        auto f = c.read();
        if (!f || f->op == kOpClose) return false;
        if (f->op == kOpPing) {
            if (!c.write(kOpPong, f->payload)) return false;
            continue;
        }
        if (f->op != kOpFrame) continue;
        const json reply = json::parse(f->payload, nullptr, false);
        if (!reply.is_discarded() && reply.value("evt", "") == "ERROR")
            error = reply.contains("data") ? reply["data"].value("message", "error") : "error";
        return true;
    }
}

} // namespace

void Presence::start(std::string application_id, Source source, core::diag::ports::Log& log) noexcept {
    if (application_id.empty()) {
        log.line("discord: no application ID in this build, Rich Presence off");
        return;
    }
    if (started_.exchange(true)) return;
    application_id_ = std::move(application_id);
    source_         = std::move(source);
    log_            = &log;
    try {
        worker_ = std::thread([this] { run(); });
        worker_.detach();
    } catch (...) {
        log.line("discord: could not start the worker thread, Rich Presence off");
    }
}

void Presence::run() noexcept {
    Connection conn;
    std::optional<Activity> sent;
    unsigned nonce        = 0;
    bool reported_missing = false;
    for (;;) {
        try {
            if (!conn.is_open()) {
                std::string error;
                if (!conn.open()) {
                    if (!reported_missing) log_->line("discord: client not running, will keep looking");
                    reported_missing = true;
                    std::this_thread::sleep_for(kRetryPeriod);
                    continue;
                }
                if (!conn.write(kOpHandshake, handshake(application_id_)) || !await_reply(conn, error) ||
                    !error.empty()) {
                    logf(*log_, "discord: handshake failed{}{}", error.empty() ? "" : ": ", error);
                    conn.close();
                    std::this_thread::sleep_for(kRetryPeriod);
                    continue;
                }
                log_->line("discord: connected");
                reported_missing = false;
                sent.reset();
            }
            const Activity now = source_();
            if (!sent || *sent != now) {
                std::string error;
                if (!conn.write(kOpFrame, set_activity(now, ++nonce)) || !await_reply(conn, error)) {
                    log_->line("discord: connection lost");
                    conn.close();
                    continue;
                }
                if (error.empty())
                    logf(*log_, "discord: activity \"{}\" / \"{}\"", now.details, now.state);
                else
                    logf(*log_, "discord: activity refused: {}", error);
                sent = now;
            }
        } catch (...) {
            // Allocation or JSON failure: drop the connection and retry later.
            conn.close();
            std::this_thread::sleep_for(kRetryPeriod);
            continue;
        }
        std::this_thread::sleep_for(kPollPeriod);
    }
}

} // namespace omvr::platform::discord
