// OpenMotherVR-Provenance: clean
// Crash-safe logging (spec 22 §14): the thread roles, the flush policy and
// schedule, the sink and the crash event.

#include "core/diag/crash_safe_sink.h"
#include "core/diag/crash_state.h"
#include "core/diag/flush_policy.h"
#include "core/diag/flush_schedule.h"
#include "core/diag/thread_roles.h"
#include "fakes/fakes.h"

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace {

using namespace std::chrono_literals;
using omvr::core::diag::CrashSafeSink;
using omvr::core::diag::CrashState;
using omvr::core::diag::flush_action;
using omvr::core::diag::flush_delay;
using omvr::core::diag::FlushAction;
using omvr::core::diag::FlushScheduler;
using omvr::core::diag::format_crash_state;
using omvr::core::diag::is_problem_line;
using omvr::core::diag::thread_name;
using omvr::core::diag::ThreadRole;
using omvr::core::diag::ThreadRoles;
using Clock = std::chrono::steady_clock;

constexpr std::uint32_t kMainId   = 100;
constexpr std::uint32_t kRenderId = 200;
constexpr std::uint32_t kModId    = 300;
constexpr std::uint32_t kJobId    = 400;

class CountingFlush final : public omvr::core::diag::ports::LogFlush {
public:
    void flush_now() noexcept override { ++now; }
    void flush_soon() noexcept override { ++soon; }
    int now  = 0;
    int soon = 0;
};

class SettableThread final : public omvr::core::diag::ports::CurrentThread {
public:
    std::uint32_t id() const noexcept override { return current; }
    std::uint32_t current = kMainId;
};

// The game, render and mod threads recorded, as the loader does.
struct KnownRoles {
    KnownRoles() {
        roles.set(ThreadRole::Main, kMainId);
        roles.set(ThreadRole::Render, kRenderId);
        roles.set(ThreadRole::Mod, kModId);
    }
    ThreadRoles roles;
};

void write(CrashSafeSink& sink, std::string_view text) { sink.write({text.data(), text.size()}); }

} // namespace

TEST_SUITE("thread roles") {
    TEST_CASE("names as 11 §1.4 writes them") {
        CHECK(thread_name(ThreadRole::Main) == "main");
        CHECK(thread_name(ThreadRole::Render) == "render");
        CHECK(thread_name(ThreadRole::Job) == "job");
        CHECK(thread_name(ThreadRole::Mod) == "mod");
        CHECK(thread_name(ThreadRole::Other) == "other");
    }

    TEST_CASE("recorded threads are told apart, others are other") {
        const KnownRoles known;
        const ThreadRoles& roles = known.roles;
        CHECK(roles.role_of(kMainId) == ThreadRole::Main);
        CHECK(roles.role_of(kRenderId) == ThreadRole::Render);
        CHECK(roles.role_of(kModId) == ThreadRole::Mod);
        CHECK(roles.role_of(kJobId) == ThreadRole::Other);
        CHECK(roles.role_of(0) == ThreadRole::Other);
    }

    TEST_CASE("nothing is known before it is recorded") {
        const ThreadRoles roles;
        CHECK(roles.role_of(kMainId) == ThreadRole::Other);
    }
}

TEST_SUITE("log flush policy") {
    TEST_CASE("a problem on the game thread is flushed at once") {
        CHECK(flush_action(ThreadRole::Main, true) == FlushAction::Now);
        CHECK(flush_action(ThreadRole::Main, false) == FlushAction::Deferred);
    }

    TEST_CASE("render and job threads never flush themselves") {
        CHECK(flush_action(ThreadRole::Render, true) == FlushAction::Deferred);
        CHECK(flush_action(ThreadRole::Job, true) == FlushAction::Deferred);
        CHECK(flush_action(ThreadRole::Other, true) == FlushAction::Deferred);
        CHECK(flush_action(ThreadRole::Mod, true) == FlushAction::Deferred);
    }

    TEST_CASE("the mod thread flushes at most once per second") {
        const Clock::time_point t0{10s};
        CHECK(flush_delay(t0, std::nullopt) == 0ms);
        CHECK(flush_delay(t0 + 300ms, t0) == 700ms);
        CHECK(flush_delay(t0 + 1000ms, t0) == 0ms);
        CHECK(flush_delay(t0 + 2500ms, t0) == 0ms);
    }

    TEST_CASE("warnings and errors are told by their wording") {
        CHECK(is_problem_line("config: saving settings to x failed err=0x00000005; the change holds"));
        CHECK(is_problem_line("[stereo] HMD backend init failed, running without VR"));
        CHECK(is_problem_line("crash: exception 0xC0000005 at 0x00401000 (AI.exe+0x1000), thread 4"));
        CHECK(is_problem_line("[hook] check failed at RVA 00001000: expected 90; found CC"));
        CHECK(is_problem_line("[hook] x: unexpected code at 0x1, not detoured"));
        CHECK(is_problem_line("openxr: WARNING game renders on another adapter"));
        CHECK(is_problem_line("ui patch: cannot write mother.pak"));
        CHECK(is_problem_line("[stereo] exception in BeginFrame caught: bad"));
    }

    TEST_CASE("ordinary state changes are not problems") {
        CHECK_FALSE(is_problem_line("present #1"));
        CHECK_FALSE(is_problem_line("[stereo] rendering configured: 4000x2000 texture=0x1234"));
        CHECK_FALSE(is_problem_line("[stereo] CreateDevice: HMD attached"));
        CHECK_FALSE(is_problem_line("config: settings saved to mothervr.json"));
        CHECK_FALSE(is_problem_line(""));
    }
}

TEST_SUITE("log flush schedule") {
    TEST_CASE("the flush thread is recorded as mod") {
        ThreadRoles roles;
        FlushScheduler schedule(roles);
        schedule.thread_started(kModId);
        CHECK(roles.role_of(kModId) == ThreadRole::Mod);
    }

    TEST_CASE("one flush per second at most") {
        ThreadRoles roles;
        FlushScheduler schedule(roles);
        const Clock::time_point t0{10s};
        CHECK(schedule.wait_before_flush(t0) == 0ms);
        schedule.flushed(t0);
        CHECK(schedule.wait_before_flush(t0 + 100ms) == 900ms);
        schedule.flushed(t0 + 1000ms);
        CHECK(schedule.wait_before_flush(t0 + 1200ms) == 800ms);
        CHECK(schedule.wait_before_flush(t0 + 2000ms) == 0ms);
    }
}

TEST_SUITE("crash-safe sink") {
    TEST_CASE("off, lines pass through and nothing is flushed") {
        omvr::tests::RecordingSink file;
        CountingFlush flush;
        SettableThread thread;
        const KnownRoles known;
        const ThreadRoles& roles = known.roles;
        CrashSafeSink sink(file, flush, thread, roles);
        write(sink, "x failed\r\n");
        write(sink, "present #1\r\n");
        CHECK(file.writes.size() == 2);
        CHECK(file.writes[0] == "x failed\r\n");
        CHECK(flush.now == 0);
        CHECK(flush.soon == 0);
    }

    TEST_CASE("on, a game-thread problem is flushed now, the rest soon") {
        omvr::tests::RecordingSink file;
        CountingFlush flush;
        SettableThread thread;
        const KnownRoles known;
        const ThreadRoles& roles = known.roles;
        CrashSafeSink sink(file, flush, thread, roles);
        sink.enable();
        CHECK(flush.soon == 1); // what was written before

        write(sink, "x failed\r\n");
        CHECK(flush.now == 1);
        write(sink, "present #1\r\n");
        CHECK(flush.now == 1);
        CHECK(flush.soon == 2);
        CHECK(file.writes.size() == 2);
    }

    TEST_CASE("on, the render thread never flushes itself") {
        omvr::tests::RecordingSink file;
        CountingFlush flush;
        SettableThread thread;
        const KnownRoles known;
        const ThreadRoles& roles = known.roles;
        CrashSafeSink sink(file, flush, thread, roles);
        sink.enable();
        thread.current = kRenderId;
        write(sink, "x failed\r\n");
        thread.current = kJobId;
        write(sink, "y failed\r\n");
        CHECK(flush.now == 0);
        CHECK(flush.soon == 3);
    }

    TEST_CASE("readiness is the file's") {
        omvr::tests::RecordingSink file;
        CountingFlush flush;
        SettableThread thread;
        const KnownRoles known;
        const ThreadRoles& roles = known.roles;
        const CrashSafeSink sink(file, flush, thread, roles);
        file.is_ready = false;
        CHECK_FALSE(sink.ready());
    }
}

TEST_SUITE("crash event") {
    TEST_CASE("conveys the thread and the states that place a crash") {
        std::array<char, 512> buf{};
        const CrashState s{.thread          = ThreadRole::Render,
                           .device_attached = true,
                           .device_state    = 2,
                           .stereo_mode     = 1,
                           .interaction     = 4,
                           .hiding          = true,
                           .screen          = 1,
                           .cutscene        = false,
                           .vr_turned_off   = false};
        const std::size_t n = format_crash_state(buf, s);
        const std::string text(buf.data(), n);
        CHECK(text.starts_with("crash:"));
        CHECK(text.find("thread render") != std::string::npos);
        CHECK(text.find("device attached (state 2)") != std::string::npos);
        CHECK(text.find("stereo mode 1") != std::string::npos);
        CHECK(text.find("interaction 4") != std::string::npos);
        CHECK(text.find("hiding yes") != std::string::npos);
        CHECK(text.find("menu screen 1") != std::string::npos);
        CHECK(text.find("VR turned off no") != std::string::npos);
    }

    TEST_CASE("is cut to the buffer") {
        std::array<char, 8> small{};
        CHECK(format_crash_state(small, CrashState{}) == small.size());
        CHECK(format_crash_state(std::span<char>{}, CrashState{}) == 0);
    }
}
