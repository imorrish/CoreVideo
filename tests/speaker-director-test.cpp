#include "speaker-director.h"
#include "zoom-types.h"

#include <iostream>
#include <vector>
#include <cassert>

static int failures = 0;

static void fail(const char *message)
{
    std::cerr << "FAIL: " << message << "\n";
    ++failures;
}

static ParticipantInfo p(uint32_t id,
                         bool has_video = true,
                         bool is_talking = false,
                         bool is_muted = false)
{
    ParticipantInfo info;
    info.user_id = id;
    info.has_video = has_video;
    info.is_talking = is_talking;
    info.is_muted = is_muted;
    return info;
}

static std::vector<ParticipantInfo> roster(std::initializer_list<ParticipantInfo> people)
{
    return std::vector<ParticipantInfo>(people);
}

static bool check_directed(uint32_t expected)
{
    if (SpeakerDirector::instance().directed_speaker_id() != expected) {
        std::cerr << "Expected directed=" << expected
                  << " got " << SpeakerDirector::instance().directed_speaker_id() << "\n";
        return false;
    }
    return true;
}

int main()
{
    auto &director = SpeakerDirector::instance();

    // --- Basic promotion ---
    director.reset();
    director.configure(500, 2000, true, {});

    uint64_t t = 10000;
    director.update_roster(roster({p(101, true, true), p(102, true, false)}), 101, t);
    if (!check_directed(101)) fail("basic promotion to raw speaker failed");

    // --- Sensitivity window prevents immediate switch ---
    director.reset();
    director.configure(500, 50, true, {});   // very short hold so we can test sensitivity cleanly
    t = 20000;
    director.update_roster(roster({p(201, true, true), p(202, true, true)}), 201, t);
    if (!check_directed(201)) fail("initial speaker not promoted");

    t += 300;
    director.update_roster(roster({p(201, true, false), p(202, true, true)}), 202, t);
    if (!check_directed(201)) fail("switched before sensitivity window");

    // Candidate became 202 at t=20300. Wait full sensitivity (hold is only 50ms).
    t += 500; // candidate_age = 500 >= 500 sensitivity, hold already satisfied
    bool switched = director.tick(t);
    if (!switched) fail("should have switched after sensitivity elapsed");
    if (!check_directed(202)) fail("failed to switch to new speaker after sensitivity");

    // --- Hold time blocks rapid switching ---
    director.reset();
    director.configure(100, 1500, true, {});
    t = 30000;
    director.update_roster(roster({p(301, true, true)}), 301, t);
    if (!check_directed(301)) fail("initial 301");

    t += 200;
    director.update_roster(roster({p(301, true, false), p(302, true, true)}), 302, t);
    t += 150; // candidate age ok, but hold not expired
    director.tick(t);
    if (!check_directed(301)) fail("switched during hold time");

    t += 1400; // now past hold
    director.tick(t);
    if (!check_directed(302)) fail("did not switch after hold expired");

    // --- Manual take supersedes everything ---
    director.reset();
    director.configure(100, 50, true, {});   // short hold
    t = 40000;
    director.update_roster(roster({p(401, true, true), p(402, true, true)}), 401, t);
    if (!check_directed(401)) fail("initial");

    t += 50;
    bool took = director.set_manual_speaker(402, t);
    if (!took) fail("manual take should succeed");
    if (!check_directed(402)) fail("manual take did not promote 402");

    // Even if raw speaker changes, manual should stick
    t += 100;
    director.update_roster(roster({p(401, true, true), p(402, true, false)}), 401, t);
    if (!check_directed(402)) fail("manual speaker was stolen by raw speaker");

    // Make the manual speaker invalid (leaves / video off) while manual is active.
    // The director should auto-clear manual and fall back.
    t += 50;
    director.update_roster(roster({p(401, true, true)}), 401, t);  // 402 no longer in roster
    if (!check_directed(401)) fail("did not auto-fallback when manual speaker became invalid");

    // --- Manual release (when still valid) keeps current directed (no unnecessary cut) ---
    director.reset();
    director.configure(100, 50, true, {});
    t = 41000;
    director.update_roster(roster({p(411, true, true), p(412, true, true)}), 411, t);
    director.set_manual_speaker(412, t);
    t += 30;
    director.clear_manual_speaker(t);
    if (!check_directed(412)) fail("clearing manual while speaker is still valid should not cut away");

    // --- Muted and no-video participants are filtered ---
    director.reset();
    director.configure(100, 1000, true, {}); // require video
    t = 50000;
    director.update_roster(roster({
        p(501, true, true),
        p(502, false, true),   // no video
        p(503, true, true, true) // muted
    }), 502, t);
    if (!check_directed(501)) fail("should have ignored no-video and muted, picked 501");

    // With require_video=false, audio-only should be allowed
    director.reset();
    director.configure(100, 1000, false, {});
    t = 51000;
    director.update_roster(roster({p(601, false, true)}), 601, t);
    if (!check_directed(601)) fail("require_video=false should allow audio-only speaker");

    // --- Exclusions work ---
    director.reset();
    director.configure(100, 1000, true, {701});
    t = 52000;
    director.update_roster(roster({p(701, true, true), p(702, true, true)}), 701, t);
    if (!check_directed(702)) fail("excluded participant 701 was still chosen");

    // --- Current directed speaker becomes invalid -> fallback ---
    director.reset();
    director.configure(100, 1000, true, {});
    t = 53000;
    director.update_roster(roster({p(801, true, true), p(802, true, false)}), 801, t);
    if (!check_directed(801)) fail("initial 801");

    t += 50;
    // 801 leaves
    director.update_roster(roster({p(802, true, false)}), 0, t);
    if (!check_directed(0)) fail("should have cleared directed when speaker left");

    // Next talking person should be promoted
    t += 100;
    director.update_roster(roster({p(802, true, true)}), 802, t);
    if (!check_directed(802)) fail("did not promote fallback speaker");

    // --- Snapshot reports reasonable timing values ---
    director.reset();
    director.configure(600, 2500, true, {});
    t = 60000;
    director.update_roster(roster({p(901, true, true)}), 901, t);
    auto snap = director.snapshot(t + 450);
    if (snap.candidate_elapsed_ms != 0) fail("should have no candidate");
    if (snap.hold_remaining_ms != 2050) fail("hold remaining calculation wrong");

    if (failures == 0) {
        std::cout << "All SpeakerDirector tests passed.\n";
        return 0;
    } else {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }
}
