// OpenMotherVR-Provenance: clean
// core/diag: the log line prefix and bounds, the logger over a sink and a
// clock, and logf's fixed buffer.

#include "core/diag/log_format.h"
#include "core/diag/logger.h"
#include "core/diag/tuning.h"
#include "fakes/fakes.h"

#include <doctest/doctest.h>

#include <array>
#include <string>

namespace {

using namespace omvr::core::diag;
using omvr::tests::FakeClock;
using omvr::tests::RecordingLog;
using omvr::tests::RecordingSink;

constexpr ports::LocalTime kTime{2024, 3, 5, 7, 8, 9, 45};

TEST_CASE("the prefix is [yyyy-MM-dd hh:mm:ss.fff pid=PPPPP]") {
    std::array<char, 64> buf{};
    const std::size_t n = format_prefix(buf, kTime, 1234);
    CHECK(std::string(buf.data(), n) == "[2024-03-05 07:08:09.045 pid=01234] ");
    const std::size_t m = format_prefix(buf, kTime, 123456);
    CHECK(std::string(buf.data(), m) == "[2024-03-05 07:08:09.045 pid=123456] ");
}

TEST_CASE("a line is prefix, message and CRLF, cut to the buffer") {
    std::array<char, kLineBufferSize> buf{};
    const std::size_t n = format_line(buf, kTime, 7, "hello");
    CHECK(std::string(buf.data(), n) == "[2024-03-05 07:08:09.045 pid=00007] hello\r\n");

    const std::string long_message(2000, 'x');
    const std::size_t cut = format_line(buf, kTime, 7, long_message);
    CHECK(cut == kLineBufferSize);
    CHECK(std::string(buf.data() + cut - 2, 2) == "\r\n");

    std::array<char, 2> tiny{};
    CHECK(format_line(tiny, kTime, 7, "x") == 0u);
}

TEST_CASE("the logger hands each line to the sink whole") {
    FakeClock clock;
    clock.set_local_time(kTime);
    RecordingSink sink;
    Logger log(clock, sink, 42);
    log.line("first");
    REQUIRE(sink.writes.size() == 1);
    CHECK(sink.writes[0] == "[2024-03-05 07:08:09.045 pid=00042] first\r\n");

    sink.is_ready = false;
    log.line("dropped");
    CHECK(sink.writes.size() == 1);
}

TEST_CASE("the logger reports the debug setting it is pointed at") {
    FakeClock clock;
    RecordingSink sink;
    Logger log(clock, sink, 1);
    CHECK_FALSE(log.debug_enabled());
    bool debug = true;
    log.set_debug_flag(&debug);
    CHECK(log.debug_enabled());
    debug = false;
    CHECK_FALSE(log.debug_enabled());
}

TEST_CASE("logf formats into a fixed buffer, cut at kMessageBufferSize - 1") {
    RecordingLog log;
    ports::logf(log, "value {} and {}", 3, "text");
    REQUIRE(log.lines.size() == 1);
    CHECK(log.lines[0] == "value 3 and text");

    const std::string big(3000, 'y');
    ports::logf(log, "{}", big);
    CHECK(log.lines[1].size() == kMessageBufferSize - 1);
}

TEST_CASE("path_text is UTF-8") {
    CHECK(path_text(L"C:\\Users\\\u00C5se\\mothervr.log") == "C:\\Users\\\xC3\x85se\\mothervr.log");
}

} // namespace
