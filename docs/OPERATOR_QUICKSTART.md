# CoreVideo Operator Quickstart

This guide is for an operator using a published CoreVideo installer. It avoids
source-build and Marketplace setup details unless they affect day-to-day use.

## Install

1. Download the latest Windows installer from GitHub Releases.
2. Close OBS Studio before running the installer.
3. Run `CoreVideo-Setup-vX.Y.Z.exe`.
4. Start OBS Studio.
5. Confirm these OBS docks/tools are available:
   - **Zoom Control**
   - **Zoom Output Manager**
   - **Zoom Diagnostics**
   - **Zoom ISO Recorder**

Published installers include the OBS plugin, `ZoomObsEngine`, Zoom SDK runtime,
Qt runtime, TLS plugins, OAuth callback helper, and locale files. End users do
not need to download the Zoom SDK or enter Zoom app credentials.

## Sign In

1. Open **Zoom Control**.
2. Click **Sign in with Zoom**.
3. Approve CoreVideo in the browser.
4. Return to OBS and confirm the status shows connected.

CoreVideo uses the published app's Public Client OAuth and Meeting SDK public
app key. There should be no prompt for a client secret in a production build.

## Join A Meeting

1. Enter the meeting ID.
2. Enter the passcode if needed.
3. Choose the join display name.
4. Click **Join**.
5. Use the visible Zoom Meeting SDK window for waiting room admit, self audio,
   self video, and normal in-meeting controls.

If join fails, open **Zoom Diagnostics** and export a support bundle before
changing settings. The bundle redacts tokens and includes OBS/engine/ISO status.

## Assign Outputs

1. Open **Zoom Output Manager**.
2. For each CoreVideo participant source, choose an assignment mode:
   - **Participant** for a fixed Zoom participant.
   - **Active Speaker** for the directed speaker feed.
   - **Spotlight Slot 1-8** for ZoomISO-style fixed stage slots.
   - **Screen share** for the active meeting share.
3. Request 1080p for production sources when the Zoom account and meeting
   entitlements support it.
4. Watch the health markers:
   - Resolution/FPS show what Zoom is actually delivering.
   - A warning marker means the observed feed does not match the requested
     output or is stale.

CoreVideo keeps OBS sources at a stable canvas size and lets OBS scale lower
quality feeds instead of changing source geometry during a meeting.

## Active Speaker

Use the **CoreVideo Active Speaker** OBS source when you want one clean output
that follows the current directed speaker.

1. Add or select **CoreVideo Active Speaker**.
2. Configure sensitivity and hold time in source properties or Zoom Control.
3. Use exclusion slots for fixed host/question-reader workflows.
4. Use manual take/release when a producer needs to override automatic switching.

The director is CoreVideo's own speaker-follow logic. It is not the same thing
as Zoom's active-speaker video feed.

## ISO Recording

1. Open **Zoom ISO Recorder**.
2. Choose an output folder.
3. Choose the video encoder:
   - `libx264` is the safest CPU fallback.
   - `h264_nvenc`, `h264_qsv`, and `h264_amf` reduce CPU load when supported by
     the installed FFmpeg build and hardware.
4. Enable **Record program** if CoreVideo should also start OBS program
   recording.
5. Click **Start ISO Recording**.

ISO recording follows assigned outputs. It records assigned participant video
to MP4 and matching audio to WAV. The panel shows requested encoder, actual
encoder, fallback state, active sessions, and file paths.

## Diagnostics And Support

Use **Zoom Diagnostics** during every production test. Export a support bundle
when you see:

- Join/auth errors.
- Engine crash or reconnect loop.
- Stale frames.
- Resolution stuck below the requested quality.
- ISO encoder failures.
- OBS close/reopen dock issues.

The bundle includes redacted settings, recent OBS log excerpts, engine status,
output health, active speaker status, screen-share status, and ISO recorder
status.

## Known Scope

The OBS plugin is the production surface today. Sidecar is tracked separately
and should not be presented as the production scene/look designer until the
Sidecar roadmap issues are complete.
