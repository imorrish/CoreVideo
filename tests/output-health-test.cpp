#include "zoom-output-health.h"

#include <iostream>
#include <vector>

static int fail(const char *message)
{
    std::cerr << message << "\n";
    return 1;
}

static ParticipantInfo participant(uint32_t id, bool has_video = true)
{
    ParticipantInfo p;
    p.user_id = id;
    p.has_video = has_video;
    return p;
}

static ZoomOutputInfo output(uint32_t participant_id = 1)
{
    ZoomOutputInfo o;
    o.assignment = AssignmentMode::Participant;
    o.participant_id = participant_id;
    o.video_resolution = VideoResolution::P1080;
    o.observed_width = 1920;
    o.observed_height = 1080;
    return o;
}

static bool expect_reason(const char *name,
                          ZoomOutputInfo output_info,
                          std::vector<ParticipantInfo> roster,
                          bool raw_media_active,
                          ZoomOutputHealthReason expected)
{
    std::vector<ZoomOutputInfo> outputs = {output_info};
    apply_output_health(outputs, roster, raw_media_active);
    if (outputs[0].health_reason != expected) {
        std::cerr << name << ": expected "
                  << output_health_reason_id(expected) << ", got "
                  << output_health_reason_id(outputs[0].health_reason) << "\n";
        return false;
    }
    return true;
}

int main()
{
    if (!expect_reason("raw media inactive", output(), {participant(1)}, false,
                       ZoomOutputHealthReason::RawMediaNotReady))
        return 1;

    if (!expect_reason("participant missing", output(99), {participant(1)}, true,
                       ZoomOutputHealthReason::ParticipantMissing))
        return 1;

    if (!expect_reason("participant video off", output(1), {participant(1, false)}, true,
                       ZoomOutputHealthReason::ParticipantVideoOff))
        return 1;

    ZoomOutputInfo share;
    share.assignment = AssignmentMode::ScreenShare;
    share.observed_width = 1920;
    share.observed_height = 1080;
    if (!expect_reason("screen share unavailable", share, {participant(1)}, true,
                       ZoomOutputHealthReason::ScreenShareUnavailable))
        return 1;

    ZoomOutputInfo waiting = output();
    waiting.observed_width = 0;
    waiting.observed_height = 0;
    if (!expect_reason("waiting for first frame", waiting, {participant(1)}, true,
                       ZoomOutputHealthReason::WaitingForFirstFrame))
        return 1;

    ZoomOutputInfo stale = output();
    stale.video_stale = true;
    if (!expect_reason("stale frame", stale, {participant(1)}, true,
                       ZoomOutputHealthReason::StaleFrame))
        return 1;

    ZoomOutputInfo low = output();
    low.observed_width = 640;
    low.observed_height = 360;
    if (!expect_reason("lower resolution", low, {participant(1)}, true,
                       ZoomOutputHealthReason::ZoomDeliveredLowerResolution))
        return 1;

    ZoomOutputInfo exact_1080 = output();
    exact_1080.video_resolution = VideoResolution::P1080;
    exact_1080.observed_width = 1920;
    exact_1080.observed_height = 1080;
    if (!expect_reason("exact 1080p is healthy", exact_1080,
                       {participant(1)}, true, ZoomOutputHealthReason::Ok))
        return 1;

    ZoomOutputInfo near_1080 = output();
    near_1080.video_resolution = VideoResolution::P1080;
    near_1080.observed_width = 1914;
    near_1080.observed_height = 1074;
    if (!expect_reason("near 1080p tolerance is healthy", near_1080,
                       {participant(1)}, true, ZoomOutputHealthReason::Ok))
        return 1;

    ZoomOutputInfo below_1080 = output();
    below_1080.video_resolution = VideoResolution::P1080;
    below_1080.observed_width = 1280;
    below_1080.observed_height = 720;
    if (!expect_reason("720p against 1080p request is lower resolution",
                       below_1080, {participant(1)}, true,
                       ZoomOutputHealthReason::ZoomDeliveredLowerResolution))
        return 1;

    if (!expect_reason("ok", output(), {participant(1)}, true,
                       ZoomOutputHealthReason::Ok))
        return 1;

    std::vector<ZoomOutputInfo> duplicates = {output(7), output(7)};
    apply_output_health(duplicates, {participant(7)}, true);
    if (duplicates[0].health_reason != ZoomOutputHealthReason::DuplicateAssignment ||
        duplicates[1].health_reason != ZoomOutputHealthReason::DuplicateAssignment ||
        !duplicates[0].duplicate_participant_assignment ||
        !duplicates[1].duplicate_participant_assignment) {
        return fail("duplicate assignment was not detected on both outputs");
    }

    // Screen share available vs unavailable
    ZoomOutputInfo share_ok;
    share_ok.assignment = AssignmentMode::ScreenShare;
    share_ok.observed_width = 1920;
    share_ok.observed_height = 1080;
    ParticipantInfo sharer;
    sharer.user_id = 99;
    sharer.is_sharing_screen = true;
    if (!expect_reason("screen share available", share_ok, {sharer}, true,
                       ZoomOutputHealthReason::Ok))
        return 1;

    // Combination: raw media inactive takes precedence over everything
    ZoomOutputInfo bad;
    bad.assignment = AssignmentMode::Participant;
    bad.participant_id = 1;
    bad.video_stale = true;
    bad.observed_width = 0;
    if (!expect_reason("raw media not ready overrides stale", bad, {participant(1)}, false,
                       ZoomOutputHealthReason::RawMediaNotReady))
        return 1;

    // Duplicate + participant missing combination (duplicate should win in current logic order)
    std::vector<ZoomOutputInfo> dup_missing = {output(88), output(88)};
    dup_missing[0].participant_id = 88;
    apply_output_health(dup_missing, {}, true); // no roster
    if (dup_missing[0].health_reason != ZoomOutputHealthReason::DuplicateAssignment ||
        dup_missing[1].health_reason != ZoomOutputHealthReason::DuplicateAssignment) {
        return fail("duplicate should be reported even when participant is also missing");
    }

    return 0;
}
