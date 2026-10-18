// OpenMotherVR-Provenance: clean
// util: hashes, parsers, the command-line splitter, the binary reader, the
// latches and the vector and pose maths. Expected values are the published
// known answers (RFC 1952, RFC 1321) or follow from the definitions.

#include "fakes/fakes.h"
#include "util/binary/align.h"
#include "util/binary/reader.h"
#include "util/binary/writer.h"
#include "util/hash/crc32.h"
#include "util/hash/md5_block.h"
#include "util/math/matrix.h"
#include "util/math/pose.h"
#include "util/math/vec.h"
#include "util/sync/change_latch.h"
#include "util/sync/frame_period.h"
#include "util/sync/once_latch.h"
#include "util/sync/seqlock.h"
#include "util/text/ascii.h"
#include "util/text/command_line_split.h"
#include "util/text/number_parse.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>
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
    std::uint32_t state[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    std::array<std::uint8_t, 64> block{};
    block[0] = 0x80; // the pad; length 0 bits
    util::md5_block(state, block.data());
    // d41d8cd98f00b204e9800998ecf8427e as little-endian words.
    CHECK(state[0] == 0xD98C1DD4u);
    CHECK(state[1] == 0x04B2008Fu);
    CHECK(state[2] == 0x980980E9u);
    CHECK(state[3] == 0x7E42F8ECu);
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
}

TEST_CASE("4x4 identity and the camera position of a rigid view matrix") {
    std::array<float, 16> v{};
    util::identity(v.data());
    CHECK(v[0] == 1.0f);
    CHECK(v[5] == 1.0f);
    CHECK(v[1] == 0.0f);
    CHECK(v[15] == 1.0f);
    // Row 3 is the view translation; with no rotation the camera sits at
    // its negation.
    v[12]                = 2.0f;
    v[13]                = -3.0f;
    v[14]                = 4.0f;
    const util::Vec3 cam = util::camera_position(v.data());
    CHECK(cam.x == -2.0f);
    CHECK(cam.y == 3.0f);
    CHECK(cam.z == -4.0f);
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
