// OpenMotherVR-Provenance: clean
// util: hashes, parsers, the command-line splitter, the binary reader, the
// latches, the seqlock, steady time, ScopeExit, units and the vector,
// quaternion and pose maths. Expected values are the published
// known answers (RFC 1952, RFC 1321) or follow from the definitions.

#include "fakes/fakes.h"
#include "util/base/scope_exit.h"
#include "util/binary/align.h"
#include "util/binary/reader.h"
#include "util/binary/writer.h"
#include "util/hash/crc32.h"
#include "util/hash/md5_block.h"
#include "util/math/finite.h"
#include "util/math/matrix.h"
#include "util/math/pose.h"
#include "util/math/quat.h"
#include "util/math/units.h"
#include "util/math/vec.h"
#include "util/sync/change_latch.h"
#include "util/sync/frame_period.h"
#include "util/sync/once_latch.h"
#include "util/sync/seqlock.h"
#include "util/text/ascii.h"
#include "util/text/command_line_split.h"
#include "util/text/fixed_format.h"
#include "util/text/number_parse.h"
#include "util/time/steady.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numbers>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace omvr;
using tests::bytes_of;

TEST_CASE("crc32 gives the zlib known answers") {
    CHECK(util::crc32(bytes_of("123456789")) == 0xCBF43926u);
    CHECK(util::crc32({}) == 0u);
    CHECK(util::crc32(bytes_of("The quick brown fox jumps over the lazy dog")) == 0x414FA339u);
}

TEST_CASE("md5_block of the padded empty message is MD5(\"\")") {
    // RFC 1321 section 3.3: A, B, C, D.
    CHECK(util::kMd5InitialState == util::Md5State{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476});
    util::Md5State state = util::kMd5InitialState;
    std::array<std::uint8_t, util::kMd5BlockBytes> block{};
    block[0] = 0x80; // the pad; length 0 bits
    util::md5_block(state, block);
    // d41d8cd98f00b204e9800998ecf8427e as little-endian words.
    CHECK(state[0] == 0xD98C1DD4u);
    CHECK(state[1] == 0x04B2008Fu);
    CHECK(state[2] == 0x980980E9u);
    CHECK(state[3] == 0x7E42F8ECu);

    // The unsized form folds the same block the same way.
    std::uint32_t words[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    util::md5_block(words, block.data());
    CHECK(std::ranges::equal(words, state));
}

TEST_CASE("parse_int takes a whole decimal integer") {
    CHECK(util::parse_int("12") == 12);
    CHECK(util::parse_int("+3") == 3);
    CHECK(util::parse_int("-4") == -4);
    CHECK_FALSE(util::parse_int("1x").has_value());
    CHECK_FALSE(util::parse_int("").has_value());
    CHECK_FALSE(util::parse_int("+").has_value());
    CHECK_FALSE(util::parse_int(nullptr).has_value());
    CHECK_FALSE(util::parse_int("99999999999").has_value());
}

TEST_CASE("parse_float takes a fixed-point number") {
    CHECK(util::parse_float("1.25") == 1.25f);
    CHECK(util::parse_float("+0.5") == 0.5f);
    CHECK(util::parse_float("2") == 2.0f);
    CHECK_FALSE(util::parse_float("1e3").has_value());
    CHECK_FALSE(util::parse_float("1.5x").has_value());
    CHECK_FALSE(util::parse_float(nullptr).has_value());
}

TEST_CASE("parse_switch takes 0 or 1 only") {
    CHECK(util::parse_switch("1") == true);
    CHECK(util::parse_switch("0") == false);
    CHECK_FALSE(util::parse_switch("2").has_value());
    CHECK_FALSE(util::parse_switch("true").has_value());
}

TEST_CASE("parse_decimal reads an ASCII wide argument") {
    CHECK(util::parse_decimal(L"0.75") == 0.75f);
    CHECK(util::parse_decimal(L"1") == 1.0f);
    CHECK_FALSE(util::parse_decimal(L"").has_value());
    CHECK_FALSE(util::parse_decimal(L"1,5").has_value());
    CHECK_FALSE(util::parse_decimal(L"0.5é").has_value());
}

TEST_CASE("the number parsers take one sign only") {
    CHECK_FALSE(util::parse_int("+-5").has_value());
    CHECK_FALSE(util::parse_int("-+5").has_value());
    CHECK_FALSE(util::parse_int("--5").has_value());
    CHECK_FALSE(util::parse_int("++5").has_value());
    CHECK_FALSE(util::parse_float("+-0.5").has_value());
    CHECK_FALSE(util::parse_float("--0.5").has_value());
    CHECK_FALSE(util::parse_decimal(L"+-0.8").has_value());
    CHECK(util::parse_float("-0.5") == -0.5f);
}

TEST_CASE("the decimal parsers refuse values that are not finite") {
    for (const char* s : {"nan", "inf", "-inf", "infinity", "+nan", "NAN", "Inf"})
        CHECK_FALSE(util::parse_float(s).has_value());
    for (const wchar_t* s : {L"nan", L"inf", L"-infinity", L"+inf"})
        CHECK_FALSE(util::parse_decimal(s).has_value());
    // Too large for a float: out of range, not infinity.
    CHECK_FALSE(util::parse_float("1000000000000000000000000000000000000000000.0").has_value());
}

TEST_CASE("parse_decimal reads the same grammar as parse_float") {
    for (const auto& [narrow, wide] :
         {std::pair{"0.8", L"0.8"}, std::pair{"+0.8", L"+0.8"}, std::pair{"-1.5", L"-1.5"},
          std::pair{"2", L"2"}, std::pair{"1e0", L"1e0"}, std::pair{".5", L".5"}, std::pair{"1.", L"1."},
          std::pair{" 1", L" 1"}}) {
        CAPTURE(narrow);
        CHECK(util::parse_decimal(wide) == util::parse_float(narrow));
    }
    CHECK_FALSE(util::parse_decimal(L"1e0").has_value()); // no exponent (06 §2: a decimal number)
    CHECK_FALSE(util::parse_decimal(nullptr).has_value());
}

TEST_CASE("split_command_line splits on spaces and tabs and honours quotes") {
    const auto t = util::split_command_line(L"  AI.exe \"a b\"\t-x  ");
    REQUIRE(t.size() == 3);
    CHECK(t[0] == L"AI.exe");
    CHECK(t[1] == L"a b");
    CHECK(t[2] == L"-x");

    const auto open = util::split_command_line(L"\"unterminated arg");
    REQUIRE(open.size() == 1);
    CHECK(open[0] == L"unterminated arg");

    CHECK(util::split_command_line(L"").empty());
    CHECK(util::split_command_line(L"   ").empty());
}

TEST_CASE("ASCII case-insensitive compare") {
    CHECK(util::equals_ignoring_ascii_case(std::wstring_view{L"-VRScale"}, std::wstring_view{L"-vrscale"}));
    CHECK_FALSE(
        util::equals_ignoring_ascii_case(std::wstring_view{L"-vrscal"}, std::wstring_view{L"-vrscale"}));
    CHECK(util::ascii_lower(L'É') == L'É'); // only ASCII letters fold
}

TEST_CASE("ASCII case-insensitive and folded search") {
    CHECK(util::find_ignoring_ascii_case("vr probe: OpenXR Failed", "failed") == 17);
    CHECK(util::contains_ignoring_ascii_case("Cannot open", "cannot"));
    CHECK_FALSE(util::contains_ignoring_ascii_case("fai", "fail"));
    CHECK(util::find_ignoring_ascii_case("abc", "") == 0);
    const auto slash = [](char c) { return c == '/' ? '\\' : util::ascii_lower(c); };
    CHECK(util::find_folded("DATA/ENV/Production/x", "env\\production\\", slash) == 5);
    CHECK(util::equals_folded("A/b", "a\\B", slash));
    CHECK_FALSE(util::equals_folded("ab", "abc", slash));
}

TEST_CASE("Reader reads little-endian values and stops at the end") {
    const std::vector<std::byte> data = {std::byte{0x78}, std::byte{0x56}, std::byte{0x34}, std::byte{0x12},
                                         std::byte{0xAB}};
    util::Reader r(data);
    CHECK(r.get<std::uint32_t>() == 0x12345678u);
    CHECK(r.ok());
    CHECK(r.get<std::uint16_t>() == 0u); // only one byte left
    CHECK_FALSE(r.ok());
    CHECK(r.bytes(1).empty()); // stays failed
}

TEST_CASE("put_u32_at writes little-endian in place; align_up rounds up") {
    std::vector<std::byte> out(6);
    util::put_u32_at(out, 1, 0xAABBCCDDu);
    CHECK(out[1] == std::byte{0xDD});
    CHECK(out[4] == std::byte{0xAA});
    CHECK(util::align_up(29, 16) == 32u);
    CHECK(util::align_up(32, 16) == 32u);
    CHECK(util::align_up(0, 4) == 0u);
}

TEST_CASE("OnceLatch, ChangeLatch and FramePeriod") {
    util::OnceLatch once;
    CHECK(once.first());
    CHECK_FALSE(once.first());

    util::ChangeLatch<int> change;
    CHECK(change.changed(1));
    CHECK_FALSE(change.changed(1));
    CHECK(change.changed(2));
    // A seeded latch does not report its seed.
    util::ChangeLatch<int> seeded(5);
    CHECK_FALSE(seeded.changed(5));
    CHECK(seeded.changed(6));

    util::FramePeriod period(3);
    CHECK_FALSE(period.tick());
    CHECK_FALSE(period.tick());
    CHECK(period.tick());
    CHECK_FALSE(period.tick());

    util::FramePeriod never(0);
    for (int i = 0; i < 5; ++i) CHECK_FALSE(never.tick());
}

TEST_CASE("SeqLock hands a value over whole") {
    struct Pair {
        float a;
        double b;
    };
    util::SeqLock<Pair> lock;
    CHECK(lock.load().a == 0.0f);
    lock.store({1.5f, 2.5});
    const Pair p = lock.load();
    CHECK(p.a == 1.5f);
    CHECK(p.b == 2.5);
}

TEST_CASE("vector maths") {
    const float half = std::numbers::pi_v<float> / 4.0f; // a quarter turn about +Y
    const util::Quat yaw90{0, std::sin(half), 0, std::cos(half)};
    const util::Vec3 r = util::rotate(yaw90, {0, 0, -1});
    CHECK(r.x == doctest::Approx(-1.0f));
    CHECK(r.y == doctest::Approx(0.0f));
    CHECK(r.z == doctest::Approx(0.0f).epsilon(1e-6));

    const util::Vec3 back = util::rotate(util::conjugate(yaw90), r);
    CHECK(back.z == doctest::Approx(-1.0f));

    const util::Vec3 d = util::sub({3, 4, 12}, {0, 0, 0});
    CHECK(util::length(d) == doctest::Approx(13.0f));
    CHECK(util::length({3, 4, 0}) == doctest::Approx(5.0f));
    CHECK(util::distance({4, 6, 12}, {1, 2, 0}) == doctest::Approx(13.0f));

    const util::Vec3 c = util::cross({0, 1, 0}, {0, 0, 1});
    CHECK(c.x == 1.0f);
    CHECK(c.y == 0.0f);
    CHECK(c.z == 0.0f);
    const util::Vec3 s = util::scaled({1, 2, 3}, 2.0f);
    CHECK(s.z == 6.0f);
}

TEST_CASE("pose maths: yaw and the 3x4 matrix") {
    const float a = 0.7f;
    const util::Quat q{0, std::sin(a / 2), 0, std::cos(a / 2)};
    CHECK(util::yaw_of(q) == doctest::Approx(a));

    float m[3][4];
    util::pose_matrix({util::Quat{}, {1, 2, 3}}, m);
    CHECK(m[0][0] == 1.0f);
    CHECK(m[1][1] == 1.0f);
    CHECK(m[2][2] == 1.0f);
    CHECK(m[0][1] == 0.0f);
    CHECK(m[0][3] == 1.0f);
    CHECK(m[1][3] == 2.0f);
    CHECK(m[2][3] == 3.0f);

    // The Matrix34 form holds the same values.
    const util::Matrix34 r = util::pose_matrix({q, {1, 2, 3}});
    util::pose_matrix({q, {1, 2, 3}}, m);
    CHECK(std::memcmp(r.m, m, sizeof(m)) == 0);
}

TEST_CASE("4x4 identity and the camera position of a rigid view matrix") {
    std::array<float, 16> v{};
    util::identity(v);
    CHECK(v[0] == 1.0f);
    CHECK(v[5] == 1.0f);
    CHECK(v[1] == 0.0f);
    CHECK(v[15] == 1.0f);
    // Row 3 is the view translation; with no rotation the camera sits at
    // its negation.
    v[12]                = 2.0f;
    v[13]                = -3.0f;
    v[14]                = 4.0f;
    const util::Vec3 cam = util::camera_position(v);
    CHECK(cam.x == -2.0f);
    CHECK(cam.y == 3.0f);
    CHECK(cam.z == -4.0f);
}

TEST_CASE("format_into: formats into the buffer, cut to fit") {
    std::array<char, 8> buf{};
    CHECK(util::format_into(buf, "{}-{}", 12, "ab") == "12-ab");
    CHECK(util::format_into(buf, "{}", "longer than eight") == "longer t");
    CHECK(util::format_into(std::span(buf).first(buf.size() - 1), "{}", 123456789) == "1234567");
    CHECK(util::format_into(std::span<char>{}, "{}", 1).empty());
}

TEST_CASE("all_finite: any infinity or NaN fails") {
    util::Matrix44 m = util::identity44();
    CHECK(util::all_finite(m));
    m[7] = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(util::all_finite(m));
    const float v[] = {0.0f, std::numeric_limits<float>::infinity()};
    CHECK_FALSE(util::all_finite(v));
    CHECK(util::all_finite(std::span<const float>{}));

    util::Matrix34 p = util::pose_matrix({util::Quat{}, {1, 2, 3}});
    CHECK(util::all_finite(p));
    p.m[2][3] = std::numeric_limits<float>::infinity();
    CHECK_FALSE(util::all_finite(p));
}

TEST_CASE("4x4 rows and columns, 3x4 translation") {
    util::Matrix44 m = util::identity44();
    CHECK(m[0] == 1.0f);
    CHECK(m[15] == 1.0f);
    util::set_row3(m, util::kTranslationRow, {2, 3, 4});
    CHECK(m[12] == 2.0f);
    CHECK(m[14] == 4.0f);
    CHECK(m[15] == 1.0f); // the fourth column is kept
    const util::Vec3 c = util::column3(m, util::kForwardAxis);
    CHECK(c.x == 0.0f);
    CHECK(c.z == 1.0f);

    util::Matrix34 p{};
    util::set_translation(p, {1, 2, 3});
    const util::Vec3 t = util::translation_of(p);
    CHECK(t.x == 1.0f);
    CHECK(t.y == 2.0f);
    CHECK(t.z == 3.0f);
}

} // namespace

TEST_CASE("steady time: stamps, offsets and the atomic stamp") {
    using namespace std::chrono_literals;
    const util::TimePoint epoch{};
    // A time at the clock's epoch is still a time: its stamp is 1, not 0.
    CHECK(util::stamp_ticks(epoch) == 1);
    CHECK_FALSE(util::from_stamp_ticks(0).has_value());
    const util::TimePoint t = epoch + 5s;
    CHECK(util::from_stamp_ticks(util::stamp_ticks(t)) == t);

    CHECK(util::after(t, util::Seconds{0.5f}) == t + 500ms);
    CHECK(util::seconds_between(t, t + 250ms).count() == doctest::Approx(0.25f));
    CHECK(util::seconds_between(t + 250ms, t).count() == doctest::Approx(-0.25f));

    util::AtomicStamp stamp;
    CHECK_FALSE(stamp.load().has_value());
    stamp.store(t);
    CHECK(stamp.load() == t);
}

TEST_CASE("ScopeExit runs once at scope end, not after release") {
    int runs = 0;
    {
        const util::ScopeExit guard{[&runs]() noexcept { ++runs; }};
        CHECK(runs == 0);
    }
    CHECK(runs == 1);
    {
        util::ScopeExit guard{[&runs]() noexcept { ++runs; }};
        guard.release();
    }
    CHECK(runs == 1);
}

TEST_CASE("Unit compares by value") {
    CHECK(util::Metres{1.5f} == util::Metres{1.5f});
    CHECK(util::Metres{1.0f} < util::Metres{2.0f});
    CHECK(util::Radians{}.value == 0.0f);
}

TEST_CASE("lerp, rotate_inverse and relative_rotation") {
    const util::Vec3 mid = util::lerp({0, 2, 4}, {2, 4, 8}, 0.5f);
    CHECK(mid.x == 1.0f);
    CHECK(mid.y == 3.0f);
    CHECK(mid.z == 6.0f);
    const util::Vec3 end = util::lerp({0, 2, 4}, {2, 4, 8}, 1.0f);
    CHECK(end.z == 8.0f);

    // A quarter turn about +Z: x goes to y.
    const float half = std::numbers::pi_v<float> / 4.0f;
    const util::Quat z90{0, 0, std::sin(half), std::cos(half)};
    const util::Matrix34 r = util::pose_matrix({z90, {5, 6, 7}});
    const util::Vec3 local = util::rotate_inverse(r, {0, 1, 0});
    CHECK(local.x == doctest::Approx(1.0f));
    CHECK(local.y == doctest::Approx(0.0f).epsilon(1e-6));
    CHECK(local.z == doctest::Approx(0.0f));

    // B's rotation in A's axes: the same pose gives identity, no translation.
    const util::Matrix34 same = util::relative_rotation(r, r);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col)
            CHECK(same.m[row][col] == doctest::Approx(row == col ? 1.0f : 0.0f).epsilon(1e-6));
    }
    // From identity to the quarter turn: the quarter turn itself.
    const util::Matrix34 from_identity = util::relative_rotation(util::pose_matrix({}), r);
    CHECK(from_identity.m[1][0] == doctest::Approx(1.0f));
    CHECK(from_identity.m[0][1] == doctest::Approx(-1.0f));
    CHECK(from_identity.m[0][3] == 0.0f);
}

TEST_CASE("quaternions: from a rotation matrix, product, nlerp, angle") {
    const auto yaw = [](float a) { return util::Quat{0, std::sin(a / 2), 0, std::cos(a / 2)}; };
    for (const float angle : {0.0f, 1.0f, 3.0f}) {
        const util::Quat z{0, 0, std::sin(angle / 2), std::cos(angle / 2)};
        const util::Quat q = util::quat_from_rotation(util::pose_matrix({z, {}}));
        CHECK(q.z == doctest::Approx(std::sin(angle / 2)).epsilon(1e-5));
        CHECK(q.w == doctest::Approx(std::cos(angle / 2)).epsilon(1e-5));
    }
    const util::Quat ab = util::multiply(yaw(0.4f), yaw(0.2f));
    CHECK(ab.y == doctest::Approx(std::sin(0.3f)));
    CHECK(util::angle_between(yaw(0.4f), yaw(0.2f)) == doctest::Approx(0.2f).epsilon(1e-4));
    const util::Quat half = util::nlerp(yaw(0), yaw(1.0f), 0.5f);
    CHECK(util::yaw_of(half) == doctest::Approx(0.5f).epsilon(1e-5));
    // The shorter arc: -q is the same rotation.
    const util::Quat neg = util::nlerp(yaw(0), util::Quat{0, -std::sin(0.5f), 0, -std::cos(0.5f)}, 0.5f);
    CHECK(util::yaw_of(neg) == doctest::Approx(0.5f).epsilon(1e-5));
}

TEST_CASE("SeqLock: a reader never sees a store half done") {
    struct Pair {
        std::uint64_t n;
        std::uint64_t not_n;
    };
    util::SeqLock<Pair> lock;
    lock.store({0, ~std::uint64_t{0}});
    // The writer keeps storing until the reader has loaded often enough to
    // overlap stores, however late it starts.
    constexpr std::uint64_t kMinStores = 100000;
    constexpr std::uint64_t kMinReads  = 10000;
    std::atomic<bool> done{false};
    std::atomic<std::uint64_t> reads{0};
    std::uint64_t torn = 0;
    std::thread reader{[&] {
        while (!done.load(std::memory_order_acquire)) {
            const Pair p = lock.load();
            if (p.not_n != ~p.n) ++torn;
            reads.fetch_add(1, std::memory_order_relaxed);
        }
    }};
    std::uint64_t n = 0;
    while (n < kMinStores || reads.load(std::memory_order_relaxed) < kMinReads) {
        ++n;
        lock.store({n, ~n});
    }
    done.store(true, std::memory_order_release);
    reader.join();
    CHECK(torn == 0);
    CHECK(lock.load().n == n);
}
