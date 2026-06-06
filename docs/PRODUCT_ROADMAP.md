# CoreVideo Product Roadmap

Last updated: 2026-06-06

This roadmap turns the current CoreVideo backlog into an execution plan. It is
ordered by end-user impact first: operators must be able to install, sign in,
join, keep video/audio stable, and recover from failures before the broader
Sidecar production-console vision can be trusted.

## Product Vision

CoreVideo should become a dependable OBS-native production layer for Zoom
meetings:

- Capture raw per-participant Zoom video, audio, and screen share without screen
  capture or virtual camera workarounds.
- Keep the operator in OBS with clear docks for joining, assigning outputs,
  monitoring health, recording ISO files, and collecting support diagnostics.
- Support up to 8 stable participant feeds at fixed OBS canvas geometry, with
  dynamic recovery when Zoom changes quality or participants come and go.
- Provide a future Sidecar production console that creates and edits real OBS
  scenes/looks, rather than a disconnected preview surface.
- Ship as a normal end-user installer with no user-entered Zoom app credentials.

## Prioritization Rules

1. **Join and media reliability first.** If users cannot join or keep feeds
   stable, no other feature matters.
2. **Operator clarity over hidden automation.** Automatic recovery is useful
   only when the UI shows what happened and what state the system is in.
3. **OBS is the source of truth.** Sidecar must create, audit, and control real
   OBS scenes and sources before it is positioned as production-ready.
4. **Release confidence must be automated.** Every release needs package,
   runtime, source, dock, and smoke-test validation.
5. **Documentation must match the product.** Public docs should not promise
   workflows that are not implemented and reliable.

## Phase 0 - Stabilize Core Production Path

Goal: make the current OBS plugin reliable enough for daily real-meeting
testing and external beta use.

| Priority | Issue | Outcome |
| --- | --- | --- |
| P0 | [#89 Stabilize Zoom sign-in and join for production Public Client OAuth](https://github.com/iamfatness/CoreVideo/issues/89) | One production auth/join path with actionable failure messages. |
| P0 | [#90 Make 8 participant video feeds stable with dynamic quality recovery](https://github.com/iamfatness/CoreVideo/issues/90) | 8-feed stability with fixed OBS geometry, stale recovery, and quality mismatch flags. |
| P0 | [#91 Fix OBS lifecycle crashes and dock reopen reliability](https://github.com/iamfatness/CoreVideo/issues/91) | Docks reopen reliably, cancel retry stops retry loops, OBS exits cleanly. |
| P0 | [#92 Ship reliable installer and release validation](https://github.com/iamfatness/CoreVideo/issues/92) | Installer/package validation catches missing Qt, SDK, FFmpeg, and helper runtime files. |

Exit criteria:

- Fresh install can sign in, join, assign sources, leave, and close OBS without a
  crash.
- Output Manager and Diagnostics can be opened, closed, and reopened in the same
  OBS session.
- A real meeting with 8 video participants can run for 30 minutes without
  unrecovered stale feeds or source geometry changes.
- Release package and installer are produced from pushed `main` and pass
  validation before upload.

## Phase 1 - Broadcast Essentials

Goal: complete the operator-critical workflows that make CoreVideo usable for
live production.

| Priority | Issue | Outcome |
| --- | --- | --- |
| P1 | [#93 Make screenshare a first-class OBS source and output assignment](https://github.com/iamfatness/CoreVideo/issues/93) | Screen share can be assigned and monitored like participant feeds. |
| P1 | [#94 Complete Active Speaker Director source and audio workflow](https://github.com/iamfatness/CoreVideo/issues/94) | Active speaker switching supports hold/sensitivity, manual supersede, exclusions, and audio routing. |
| P1 | [#95 Finish ISO recording manager with GPU encode strategy and capacity guidance](https://github.com/iamfatness/CoreVideo/issues/95) | ISO recording is manageable, observable, and realistic about CPU/GPU/storage limits. |
| P1 | [#96 Improve Output Manager as the primary operator surface](https://github.com/iamfatness/CoreVideo/issues/96) | Output Manager becomes the main assignment/health panel with no clipped state. |
| P1 | [#97 Build support bundles and production diagnostics](https://github.com/iamfatness/CoreVideo/issues/97) | One-click redacted support bundles for auth, join, video, audio, and runtime issues. |

Exit criteria:

- Operators can manage participant, active speaker, spotlight, screen share, and
  isolated-audio assignments without opening source properties.
- Active speaker workflows support host/question-reader exclusion and clean cuts.
- ISO recording can start/stop for assigned feeds without orphaned encoder
  processes.
- Diagnostics exports enough information to debug support cases without
  requesting manual log digging first.

## Phase 2 - Production Console and Scene Design

Goal: make Sidecar a real OBS scene/look designer and production console.

| Priority | Issue | Outcome |
| --- | --- | --- |
| P2 | [#98 Rebuild Sidecar around real OBS scene/look control](https://github.com/iamfatness/CoreVideo/issues/98) | Sidecar applies changes to actual OBS scenes and audits mismatches. |
| P2 | [#99 Add Sidecar look designer with backgrounds, overlays, and template save/load](https://github.com/iamfatness/CoreVideo/issues/99) | Operators can design 16:9 looks with slots, images, overlays, opacity, shadows, and rounded corners. |
| P2 | [#100 Restore participant mapping roster and pop-out management window](https://github.com/iamfatness/CoreVideo/issues/100) | Placeholder and live Zoom participant mapping works predictably. |
| P2 | [#101 Participant-synced lower thirds and overlay automation](https://github.com/iamfatness/CoreVideo/issues/101) | Lower thirds and overlays follow live assignments with manual overrides. |

Exit criteria:

- Applying any Sidecar look visibly updates OBS.
- Sidecar can create or repair required OBS scenes, nested participant scenes,
  source items, overlays, and layer ordering.
- Built-in templates cover 1-8 sources plus speaker + screenshare.
- Preview state is either WYSIWYG from OBS or clearly marked as design-only.

## Phase 3 - Scale, Performance, and Automation

Goal: build confidence that CoreVideo can survive production load and repeated
releases.

| Priority | Issue | Outcome |
| --- | --- | --- |
| P3 | [#102 Build repeatable load tests for 8 feeds, ISO recording, and stream output](https://github.com/iamfatness/CoreVideo/issues/102) | Objective CPU/GPU/memory/dropped-frame/storage data for production scenarios. |
| P3 | [#103 Automate OBS scene/source smoke tests](https://github.com/iamfatness/CoreVideo/issues/103) | Local and CI tests catch plugin, dock, source, scene, and package regressions. |
| P3 | [#88 Code quality findings from Cppcheck](https://github.com/iamfatness/CoreVideo/issues/88) | Static-analysis findings are triaged and fixed continuously. |

Exit criteria:

- A repeatable load-test report exists for 8 feeds, 8 ISO records, and a program
  stream.
- Local release workflow and GitHub Actions run useful smoke checks before
  publishing assets.
- Code quality findings are tracked as normal backlog work, not release-week
  surprises.

## Phase 4 - Commercial Release Readiness

Goal: make CoreVideo understandable and supportable for external users.

| Priority | Issue | Outcome |
| --- | --- | --- |
| P3 | [#104 Production onboarding, marketplace readiness, and public documentation](https://github.com/iamfatness/CoreVideo/issues/104) | Website, README, release notes, installer, and screenshots match current product behavior. |

Exit criteria:

- End users do not need to provide Zoom app credentials.
- Public docs explain install, sign-in, join, source assignment, ISO recording,
  diagnostics, and known limitations.
- Sidecar documentation is gated until Sidecar is production-ready.
- Release checklist includes website deployment and screenshot refresh.

## Execution Rhythm

- Work top-down by phase unless a lower-phase item directly blocks testing.
- Keep each GitHub issue focused on one operator-visible outcome.
- For large issues, create child implementation issues only after the first
  design pass identifies concrete tasks.
- Every completed issue should update docs, diagnostics, or tests when the
  change affects user workflows.
- A release should not ship with known regressions in Phase 0 acceptance
  criteria.

## Current Next Best Work

1. Start with [#89](https://github.com/iamfatness/CoreVideo/issues/89) and
   [#90](https://github.com/iamfatness/CoreVideo/issues/90) together because
   join reliability and video-feed reliability are the product foundation.
2. Use [#91](https://github.com/iamfatness/CoreVideo/issues/91) as the hardening
   lane while testing #89/#90 in real OBS sessions.
3. Do not resume major Sidecar feature work until Phase 0 is demonstrably stable
   and Phase 1 operator surfaces are usable.
