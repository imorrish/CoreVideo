#pragma once

#include "zoom-output-manager.h"
#include <algorithm>
#include <unordered_map>
#include <vector>

inline void apply_output_health(std::vector<ZoomOutputInfo> &outputs,
                                const std::vector<ParticipantInfo> &roster,
                                bool raw_media_active)
{
    std::unordered_map<uint32_t, size_t> assigned_counts;
    for (const auto &output : outputs) {
        if (output.assignment == AssignmentMode::Participant &&
            output.participant_id != 0) {
            ++assigned_counts[output.participant_id];
        }
    }

    for (auto &output : outputs) {
        output.duplicate_participant_assignment = false;
        output.health_reason = ZoomOutputHealthReason::Ok;
        if (output.assignment == AssignmentMode::Participant &&
            output.participant_id != 0) {
            output.duplicate_participant_assignment =
                assigned_counts[output.participant_id] > 1;
        }
    }

    const bool screen_share_available = std::any_of(
        roster.begin(), roster.end(),
        [](const ParticipantInfo &participant) {
            return participant.is_sharing_screen;
        });
    const bool active_video_speaker_available = std::any_of(
        roster.begin(), roster.end(),
        [](const ParticipantInfo &participant) {
            return participant.is_talking && participant.has_video;
        });

    auto find_participant = [&roster](uint32_t participant_id) {
        return std::find_if(roster.begin(), roster.end(),
            [participant_id](const ParticipantInfo &participant) {
                return participant.user_id == participant_id;
            });
    };

    for (auto &output : outputs) {
        const bool wants_media =
            output.assignment == AssignmentMode::Participant ||
            output.assignment == AssignmentMode::ActiveSpeaker ||
            output.assignment == AssignmentMode::SpotlightIndex ||
            output.assignment == AssignmentMode::ScreenShare;
        if (!raw_media_active) {
            output.health_reason = wants_media
                ? ZoomOutputHealthReason::RawMediaNotReady
                : ZoomOutputHealthReason::Ok;
        } else if (output.duplicate_participant_assignment) {
            output.health_reason = ZoomOutputHealthReason::DuplicateAssignment;
        } else if (output.assignment == AssignmentMode::ScreenShare &&
                   !screen_share_available) {
            output.health_reason = ZoomOutputHealthReason::ScreenShareUnavailable;
        } else if (output.assignment == AssignmentMode::ActiveSpeaker &&
                   output.participant_id == 0 &&
                   !active_video_speaker_available) {
            output.health_reason = ZoomOutputHealthReason::ActiveSpeakerUnavailable;
        } else if (output.assignment == AssignmentMode::SpotlightIndex &&
                   output.spotlight_slot > 0) {
            const auto spotlight_it = std::find_if(
                roster.begin(), roster.end(),
                [&output](const ParticipantInfo &participant) {
                    return participant.spotlight_index == output.spotlight_slot;
                });
            if (spotlight_it == roster.end()) {
                output.health_reason = ZoomOutputHealthReason::SpotlightUnavailable;
            } else if (!spotlight_it->has_video) {
                output.health_reason = ZoomOutputHealthReason::ParticipantVideoOff;
            }
        } else if (output.assignment == AssignmentMode::Participant &&
                   output.participant_id != 0) {
            const auto participant_it = find_participant(output.participant_id);
            if (participant_it == roster.end()) {
                output.health_reason = ZoomOutputHealthReason::ParticipantMissing;
            } else if (!participant_it->has_video) {
                output.health_reason = ZoomOutputHealthReason::ParticipantVideoOff;
            }
        }

        if (output.health_reason != ZoomOutputHealthReason::Ok)
            continue;
        if (output.video_stale) {
            output.health_reason = ZoomOutputHealthReason::StaleFrame;
        } else if (output.observed_width == 0 || output.observed_height == 0) {
            output.health_reason = ZoomOutputHealthReason::WaitingForFirstFrame;
        } else if (output_signal_below_requested(output)) {
            output.health_reason = ZoomOutputHealthReason::ZoomDeliveredLowerResolution;
        }
    }
}
