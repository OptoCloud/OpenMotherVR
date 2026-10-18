// OpenMotherVR-Provenance: clean
// core/hands body features (spec 21 §10 to §12). Expected values are the
// spec's: the bone groups of 21 §2.3 and the Body.Visible table of §10.2,
// the collapse rule of §10.2, the real crouch thresholds 0.78 h and 0.86 h,
// the 100 ms press and 0.5 s retry of §11.5, the 0.5 s settle of §12.2 item
// 3, the game's crouch drop of 0.63 m from its own data (§11.5), and the
// offsets in runtime metres before the scale (§11.1).

#include "core/hands/body_controls.h"
#include "core/hands/body_hider.h"
#include "core/hands/body_tuning.h"
#include "core/hands/body_visibility.h"
#include "core/hands/hand_back.h"
#include "core/hands/real_crouch.h"
#include "core/stereo/play_space.h"
#include "core/vr/pose_convert.h"
#include "fakes/fakes.h"

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

using namespace omvr::core::hands;
using omvr::core::config::BodyVisible;
using omvr::tests::FakeClock;
using omvr::tests::RecordingLog;
using namespace std::chrono_literals;
using Clock = RealCrouch::Clock;

// A first-person skeleton shaped like the game's (21 §2.3): names with the
// tools' namespace prefix, fingers and a weapon bone under each hand.
struct TestSkeleton {
    std::vector<std::string_view> names;
    std::vector<std::int16_t> parents;

    std::int16_t add(std::string_view name, std::int16_t parent) {
        names.push_back(name);
        parents.push_back(parent);
        return static_cast<std::int16_t>(names.size() - 1);
    }
    SkeletonBones bones() const { return {names, parents}; }
    std::size_t index(std::string_view name) const {
        for (std::size_t i = 0; i < names.size(); ++i)
            if (bare_bone_name(names[i]) == name) return i;
        FAIL("no bone ", name);
        return 0;
    }
};

TestSkeleton female_fp() {
    TestSkeleton s;
    const auto hips = s.add("FemaleFP:Hips", -1);
    s.add("FemaleFP:Pelvis", hips);
    const auto lleg = s.add("FemaleFP:LeftUpLeg", hips);
    s.add("FemaleFP:LeftLeg", lleg);
    const auto rleg = s.add("FemaleFP:RightUpLeg", hips);
    s.add("FemaleFP:RightLeg", rleg);
    const auto spine  = s.add("FemaleFP:Spine", hips);
    const auto spine1 = s.add("FemaleFP:Spine1", spine);
    const auto spine2 = s.add("FemaleFP:Spine2", spine1);
    const auto neck   = s.add("FemaleFP:Neck", spine2);
    s.add("FemaleFP:Head", neck);
    for (const auto side : {std::string_view{"Left"}, std::string_view{"Right"}}) {
        const bool left    = side == "Left";
        const auto sh      = s.add(left ? "FemaleFP:LeftShoulder" : "FemaleFP:RightShoulder", spine2);
        const auto arm     = s.add(left ? "FemaleFP:LeftArm" : "FemaleFP:RightArm", sh);
        const auto forearm = s.add(left ? "FemaleFP:LeftForeArm" : "FemaleFP:RightForeArm", arm);
        const auto roll = s.add(left ? "FemaleFP:LeftForeArmRoll1" : "FemaleFP:RightForeArmRoll1", forearm);
        const auto hand = s.add(left ? "FemaleFP:LeftHand" : "FemaleFP:RightHand", roll);
        s.add(left ? "FemaleFP:LeftHandIndex1" : "FemaleFP:RightHandIndex1", hand);
        s.add(left ? "FemaleFP:LeftWeaponBone" : "FemaleFP:RightWeaponBone", hand);
    }
    return s;
}

std::vector<BoneTransform> rest_pose(std::size_t n) {
    std::vector<BoneTransform> p(n);
    for (std::size_t i = 0; i < n; ++i) {
        const float f = static_cast<float>(i);
        p[i]          = {{f, 2.0f * f, 3.0f * f, 0.0f}, {0.1f, 0.2f, 0.3f, 0.9f}, {1.0f, 1.0f, 1.0f, 1.0f}};
    }
    return p;
}

bool kept(const CollapsePlan& plan, std::size_t i) { return plan.bones[i].anchor_count == 0; }

} // namespace

TEST_CASE("body groups: the game's bone names, with or without the tools' prefix") {
    CHECK(bare_bone_name("FemaleFP:LeftHand") == "LeftHand");
    CHECK(bare_bone_name("SpaceSuitFP:Spine2") == "Spine2");
    CHECK(bare_bone_name("Head") == "Head");
    CHECK(listed_group("FemaleFP:LeftHand") == BoneGroup::Hand);
    CHECK(listed_group("RightForeArmRoll1") == BoneGroup::Arm);
    CHECK(listed_group("LeftShoulder") == BoneGroup::Shoulder);
    CHECK(listed_group("RightWeaponBone") == BoneGroup::Weapon);
    CHECK(listed_group("LeftToeBase") == BoneGroup::Body);
    CHECK_FALSE(listed_group("LeftHandIndex1").has_value()); // a finger takes its hand's group
}

TEST_CASE("body visibility: what each Body.Visible value draws (21 §10.2)") {
    CHECK(group_visible(BoneGroup::Body, BodyVisible::Full));
    CHECK_FALSE(group_visible(BoneGroup::Body, BodyVisible::ArmsAndHands));
    CHECK(group_visible(BoneGroup::Arm, BodyVisible::ArmsAndHands));
    CHECK(group_visible(BoneGroup::Shoulder, BodyVisible::ArmsAndHands));
    CHECK_FALSE(group_visible(BoneGroup::Arm, BodyVisible::HandsOnly));
    CHECK(group_visible(BoneGroup::Hand, BodyVisible::HandsOnly));
    CHECK_FALSE(group_visible(BoneGroup::Hand, BodyVisible::None));
    for (const auto v :
         {BodyVisible::Full, BodyVisible::ArmsAndHands, BodyVisible::HandsOnly, BodyVisible::None})
        CHECK(group_visible(BoneGroup::Weapon, v)); // the held item is always drawn
}

TEST_CASE("collapse plan: Full hides nothing") {
    const auto s = female_fp();
    CollapsePlan plan;
    REQUIRE(build_collapse_plan(s.bones(), BodyVisible::Full, plan) == PlanResult::Ok);
    CHECK(plan.count == s.names.size());
    CHECK(plan.hidden == 0);
}

TEST_CASE("collapse plan: Arms and Hands hides the body into the shoulders' joints") {
    const auto s = female_fp();
    CollapsePlan plan;
    REQUIRE(build_collapse_plan(s.bones(), BodyVisible::ArmsAndHands, plan) == PlanResult::Ok);
    for (const auto name : {"LeftShoulder", "RightArm", "LeftHand", "RightHandIndex1", "LeftWeaponBone"})
        CHECK(kept(plan, s.index(name)));
    for (const auto name : {"Hips", "Pelvis", "Spine2", "Neck", "Head", "LeftLeg"})
        CHECK_FALSE(kept(plan, s.index(name)));
    // Spine2 is equally near both shoulders, which hang below it: both are
    // anchors, so it collapses midway between the shoulder joints.
    const BoneCollapse& c = plan.bones[s.index("Spine2")];
    REQUIRE(c.anchor_count == 2);
    CHECK(static_cast<std::size_t>(c.anchors[0]) == s.index("LeftShoulder"));
    CHECK(static_cast<std::size_t>(c.anchors[1]) == s.index("RightShoulder"));
    // The legs too: every hidden bone collapses at that one point.
    const BoneCollapse& leg = plan.bones[s.index("LeftLeg")];
    CHECK(leg.anchor_count == 2);
}

TEST_CASE("collapse plan: Hands Only shrinks each arm into its wrist") {
    const auto s = female_fp();
    CollapsePlan plan;
    REQUIRE(build_collapse_plan(s.bones(), BodyVisible::HandsOnly, plan) == PlanResult::Ok);
    CHECK(kept(plan, s.index("LeftHand")));
    CHECK(kept(plan, s.index("RightHandIndex1")));
    for (const auto name : {"LeftShoulder", "LeftArm", "LeftForeArm", "LeftForeArmRoll1"}) {
        const BoneCollapse& c = plan.bones[s.index(name)];
        REQUIRE(c.anchor_count == 1);
        CHECK(static_cast<std::size_t>(c.anchors[0]) == s.index("LeftHand"));
    }
    CHECK(static_cast<std::size_t>(plan.bones[s.index("RightArm")].anchors[0]) == s.index("RightHand"));
}

TEST_CASE("collapse plan: None keeps only the weapon bones") {
    const auto s = female_fp();
    CollapsePlan plan;
    REQUIRE(build_collapse_plan(s.bones(), BodyVisible::None, plan) == PlanResult::Ok);
    CHECK(plan.hidden == s.names.size() - 2);
    CHECK(kept(plan, s.index("LeftWeaponBone")));
    CHECK(kept(plan, s.index("RightWeaponBone")));
    const BoneCollapse& hand = plan.bones[s.index("LeftHand")];
    REQUIRE(hand.anchor_count == 1);
    CHECK(static_cast<std::size_t>(hand.anchors[0]) == s.index("LeftWeaponBone"));
}

TEST_CASE("collapse plan: below a drawn bone, at the hidden bone's own joint") {
    auto s            = female_fp();
    const auto weapon = static_cast<std::int16_t>(s.index("LeftWeaponBone"));
    const auto extra  = s.add("FemaleFP:Head1", weapon); // a body bone hanging from a drawn one
    s.add("FemaleFP:Pelvis", extra);
    CollapsePlan plan;
    REQUIRE(build_collapse_plan(s.bones(), BodyVisible::HandsOnly, plan) == PlanResult::Ok);
    const BoneCollapse& below = plan.bones[s.names.size() - 1];
    REQUIRE(below.anchor_count == 1);
    CHECK(below.anchors[0] == extra); // the joint the hidden part hangs from
    CHECK(plan.bones[static_cast<std::size_t>(extra)].anchors[0] == extra);
}

TEST_CASE("collapse plan: a skeleton it cannot plan is reported") {
    CollapsePlan plan;
    SUBCASE("a bone it needs is missing by name") {
        TestSkeleton s;
        s.add("FemaleFP:Hips", -1);
        s.add("FemaleFP:Spine", 0);
        CHECK(build_collapse_plan(s.bones(), BodyVisible::HandsOnly, plan) == PlanResult::MissingBones);
        CHECK(first_missing_bone(s.bones()) == "LeftShoulder");
    }
    SUBCASE("parent indices out of range or in a loop") {
        auto s       = female_fp();
        s.parents[0] = static_cast<std::int16_t>(s.names.size());
        CHECK(build_collapse_plan(s.bones(), BodyVisible::None, plan) == PlanResult::BadParents);
        s.parents[0] = 1; // Pelvis's parent is Hips: a loop
        CHECK(build_collapse_plan(s.bones(), BodyVisible::None, plan) == PlanResult::BadParents);
    }
    SUBCASE("more bones than the plan holds") {
        std::vector<std::string_view> names(kMaxBodyBones + 1, "Spine");
        std::vector<std::int16_t> parents(kMaxBodyBones + 1, -1);
        CHECK(build_collapse_plan({names, parents}, BodyVisible::None, plan) == PlanResult::TooManyBones);
    }
    CHECK(plan.count == 0);
}

TEST_CASE("apply: hidden bones collapse at their anchors, drawn ones keep the game's pose") {
    const auto s = female_fp();
    CollapsePlan plan;
    REQUIRE(build_collapse_plan(s.bones(), BodyVisible::ArmsAndHands, plan) == PlanResult::Ok);
    const auto src = rest_pose(s.names.size());
    std::vector<BoneTransform> dst(src.size());
    apply_collapse_plan(plan, src, dst);

    const std::size_t hand = s.index("LeftHand");
    CHECK(dst[hand].translation == src[hand].translation);
    CHECK(dst[hand].rotation == src[hand].rotation);
    CHECK(dst[hand].scale == src[hand].scale);

    const std::size_t neck = s.index("Neck");
    const auto& l          = src[s.index("LeftShoulder")].translation;
    const auto& r          = src[s.index("RightShoulder")].translation;
    for (std::size_t k = 0; k < 3; ++k) {
        CHECK(dst[neck].translation[k] == doctest::Approx((l[k] + r[k]) / 2.0f));
        CHECK(dst[neck].scale[k] == kCollapseScale);
        CHECK(dst[neck].rotation[k] == 0.0f);
    }
    CHECK(dst[neck].rotation[3] == 1.0f);
}

TEST_CASE("body hider: publishes a plan only while something is hidden and the device runs") {
    RecordingLog log;
    BodyHider hider{log};
    const auto s     = female_fp();
    const auto bones = s.bones();
    const PlayerSkeleton player{0x1000, 0x2000, 0x3000, "FEMALEFP"};

    CHECK_FALSE(hider.wants_bones(BodyVisible::Full, player)); // the default reads nothing
    hider.update(BodyVisible::Full, true, player, nullptr);
    CHECK(hider.current() == nullptr);

    REQUIRE(hider.wants_bones(BodyVisible::HandsOnly, player));
    hider.update(BodyVisible::HandsOnly, true, player, &bones);
    const HiddenPose* p = hider.current();
    REQUIRE(p != nullptr);
    CHECK(p->pose == 0x2000);
    CHECK(p->skeleton == 0x3000);
    CHECK(p->plan.hidden > 0);
    CHECK(log.contains("[body] player skeleton FEMALEFP"));
    CHECK(log.contains("[body] visibility HandsOnly"));

    // The same skeleton and value: no new read, the same plan.
    CHECK_FALSE(hider.wants_bones(BodyVisible::HandsOnly, player));
    hider.update(BodyVisible::HandsOnly, true, player, nullptr);
    CHECK(hider.current() == p);

    // Without a running device nothing is hidden (20 §1.2).
    hider.update(BodyVisible::HandsOnly, false, player, nullptr);
    CHECK(hider.current() == nullptr);

    // Back to Full: nothing hidden, and that is said once.
    hider.update(BodyVisible::Full, true, player, nullptr);
    CHECK(hider.current() == nullptr);
    CHECK(log.contains("[body] visibility Full"));
}

TEST_CASE("body hider: a skeleton without the bones is drawn whole and logged once") {
    RecordingLog log;
    BodyHider hider{log};
    TestSkeleton s;
    s.add("DallasFP:Hips", -1);
    const auto bones = s.bones();
    const PlayerSkeleton player{0x1000, 0x2000, 0x3000, "DALLASFP"};
    hider.update(BodyVisible::None, true, player, &bones);
    CHECK(hider.current() == nullptr);
    CHECK(log.contains("no bone Spine by name"));
    const auto lines = log.lines.size();
    hider.update(BodyVisible::None, true, player, &bones);
    CHECK(log.lines.size() == lines);
}

namespace {

// 21 §11.5: h 1.70 m; crouch below 1.326 m, stand above 1.462 m.
constexpr float kH = 1.7f;

RealCrouchInput standing_at(float head, std::optional<bool> crouched) {
    RealCrouchInput in;
    in.enabled               = true;
    in.standing              = true;
    in.standing_eye_height_m = kH;
    in.head_height_m         = head;
    in.controllable          = true;
    in.game_crouched         = crouched;
    return in;
}

} // namespace

TEST_CASE("real crouch: a real crouch presses the game's crouch once, and the view stays at the head") {
    RealCrouch rc;
    Clock::time_point t{10s};
    auto out = rc.update(standing_at(1.70f, false), t);
    CHECK(out.press == CrouchPress::None);
    CHECK(out.crouch_below_m == doctest::Approx(0.78f * kH));
    CHECK(out.stand_above_m == doctest::Approx(0.86f * kH));

    out = rc.update(standing_at(1.40f, false), t += 100ms); // between the thresholds: nothing
    CHECK(out.press == CrouchPress::None);
    out = rc.update(standing_at(1.20f, false), t += 100ms);
    CHECK(out.press == CrouchPress::Crouch);
    CHECK(out.eye_raise_m == 0.0f); // the game has not crouched yet

    // The game crouches: no further press, and the raise follows its drop.
    out = rc.update(standing_at(1.20f, true), t += 100ms);
    CHECK(out.press == CrouchPress::None);
    out = rc.update(standing_at(1.20f, true), t += 100ms);
    out = rc.update(standing_at(1.20f, true), t += 100ms);
    out = rc.update(standing_at(1.20f, true), t += 100ms);
    CHECK(out.eye_raise_m == doctest::Approx(kGameCrouchEyeDropM));

    // Standing up: one stand press; the game stands and the raise goes.
    out = rc.update(standing_at(1.50f, true), t += 100ms);
    CHECK(out.press == CrouchPress::Stand);
    for (int i = 0; i < 4; ++i) out = rc.update(standing_at(1.70f, false), t += 100ms);
    CHECK(out.press == CrouchPress::None);
    CHECK(out.eye_raise_m == 0.0f);
}

TEST_CASE("real crouch: standing refused in a vent is retried every 0.5 s") {
    RealCrouch rc;
    Clock::time_point t{10s};
    rc.update(standing_at(1.70f, false), t);
    CHECK(rc.update(standing_at(1.20f, false), t += 100ms).press == CrouchPress::Crouch);
    rc.update(standing_at(1.20f, true), t += 100ms); // followed
    // No second press within 0.5 s of the first.
    CHECK(rc.update(standing_at(1.70f, true), t += 100ms).press == CrouchPress::None);
    CHECK(rc.update(standing_at(1.70f, true), t += 300ms).press == CrouchPress::Stand);
    CHECK(rc.update(standing_at(1.70f, true), t += 400ms).press == CrouchPress::None);
    CHECK(rc.update(standing_at(1.70f, true), t += 100ms).press == CrouchPress::Stand);
}

TEST_CASE("real crouch: a press the game does not follow is not repeated, and is reported") {
    RealCrouch rc;
    Clock::time_point t{10s};
    rc.update(standing_at(1.70f, false), t);
    CHECK(rc.update(standing_at(1.20f, false), t += 100ms).press == CrouchPress::Crouch);
    bool reported = false;
    for (int i = 0; i < 15; ++i) {
        const auto out = rc.update(standing_at(1.20f, false), t += 100ms);
        CHECK(out.press == CrouchPress::None);
        reported = reported || out.press_not_followed;
    }
    CHECK(reported);
    // Standing up: the byte never followed a press, so no stand retries.
    CHECK(rc.update(standing_at(1.70f, false), t += 100ms).press == CrouchPress::None);
}

TEST_CASE("real crouch: the player's own crouch control is not fought") {
    RealCrouch rc;
    Clock::time_point t{10s};
    rc.update(standing_at(1.70f, false), t);
    // Crouched by the control while standing for real: no press, no raise.
    for (int i = 0; i < 10; ++i) {
        const auto out = rc.update(standing_at(1.70f, true), t += 100ms);
        CHECK(out.press == CrouchPress::None);
        CHECK(out.eye_raise_m == 0.0f);
    }
}

TEST_CASE("real crouch: only in standing play with a calibration and a head height") {
    RealCrouch rc;
    Clock::time_point t{10s};
    auto in     = standing_at(1.70f, false);
    in.standing = false; // seated
    rc.update(in, t);
    in.head_height_m = 1.0f;
    CHECK(rc.update(in, t += 100ms).press == CrouchPress::None);
    CHECK_FALSE(rc.active());
    in                       = standing_at(1.0f, false);
    in.standing_eye_height_m = 0.0f; // not calibrated
    CHECK(rc.update(in, t += 100ms).press == CrouchPress::None);
    in               = standing_at(1.0f, false);
    in.head_height_m = std::nullopt; // headset not in use
    CHECK(rc.update(in, t += 100ms).press == CrouchPress::None);
    in         = standing_at(1.0f, false);
    in.enabled = false;
    CHECK(rc.update(in, t += 100ms).press == CrouchPress::None);
}

TEST_CASE("real crouch: no presses during an episode; re-synced 0.5 s after it ends") {
    RealCrouch rc;
    Clock::time_point t{10s};
    rc.update(standing_at(1.70f, false), t);
    auto in         = standing_at(1.20f, false);
    in.controllable = false; // a cutscene
    CHECK(rc.update(in, t += 100ms).press == CrouchPress::None);
    CHECK(rc.update(in, t += 100ms).press == CrouchPress::None);
    rc.on_hand_back(t);
    CHECK(rc.update(standing_at(1.20f, false), t += 400ms).press == CrouchPress::None);
    CHECK(rc.update(standing_at(1.20f, false), t += 100ms).press == CrouchPress::Crouch);
    CHECK(rc.update(standing_at(1.20f, false), t += 600ms).press == CrouchPress::None); // one press only
}

TEST_CASE("hand-back: the episode that ended, once play resumes") {
    HandBack hb;
    CHECK_FALSE(hb.update(Activity::Active).has_value());
    CHECK_FALSE(hb.update(Activity::Cutscene).has_value());
    CHECK(hb.in_episode());
    CHECK(hb.update(Activity::Active) == Activity::Cutscene);
    CHECK_FALSE(hb.in_episode());
    // A death that loads is a death.
    hb.update(Activity::Dead);
    hb.update(Activity::MenuScreen);
    CHECK(hb.update(Activity::Active) == Activity::Dead);
    // The device or a setting off is not an episode.
    hb.update(Activity::NoStereo);
    CHECK_FALSE(hb.update(Activity::Active).has_value());
}

namespace {

class RecordingControls final : public omvr::core::input::ports::ControlsSink {
public:
    void write(const omvr::core::input::Controls& c, bool) noexcept override { last = c; }
    void set_scheme(omvr::core::input::ControlScheme) noexcept override {}
    omvr::core::input::Controls last{};
};

} // namespace

TEST_CASE("body controls: a crouch press is added to the Crouch control for its time") {
    RecordingControls next;
    FakeClock clock;
    BodyControls bc{next, clock};
    omvr::core::input::Controls c;
    c.trigger_l = 0.2f;
    bc.write(c, false);
    CHECK(next.last.trigger_l == 0.2f); // passes through unchanged
    bc.press_crouch_until(clock.now() + 100ms);
    bc.write(c, false);
    CHECK(next.last.trigger_l == 1.0f);
    clock.advance(100ms);
    bc.write(c, false);
    CHECK(next.last.trigger_l == 0.2f);
}

TEST_CASE("play space: real crouch's raise is game metres at any world scale") {
    using namespace omvr::core::stereo;
    RecordingLog log;
    for (const float scale : {1.0f, 2.0f}) {
        PlaySpace ps{log};
        PlaySpaceSettings s;
        s.world_scale_setting = scale;
        ps.begin_frame(s);
        const float raw[3][4] = {
            {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.6f}, {0.0f, 0.0f, 1.0f, 0.0f}};
        const auto head    = omvr::core::vr::to_game_pose(raw, 1.0f);
        const auto ref     = ps.on_reference(head, 0.0f, "OpenVR");
        const float before = ps.on_head_pose(head, 0.0f).game.m[1][3] - ref.m[1][3];
        ps.set_crouch_raise(kGameCrouchEyeDropM);
        const auto poses = ps.on_head_pose(head, 0.0f);
        CHECK(poses.game.m[1][3] - ref.m[1][3] - before == doctest::Approx(-kGameCrouchEyeDropM)); // up is -y
        CHECK(poses.player.m[1][3] - ref.m[1][3] == doctest::Approx(before)); // not the player's movement
    }
}
