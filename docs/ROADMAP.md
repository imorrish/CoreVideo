# CoreVideo Roadmap

> Where we're heading. This roadmap is a living document - items shift as
> we learn from production use. Horizons are directional, not commitments.
> Everything here is subject to change.

CoreVideo is an OBS Studio plugin that pulls per-participant raw video,
audio, screen share, and interpretation channels out of a Zoom meeting via
the Zoom Meeting SDK - no screen capture, no virtual camera. The current
production path is the OBS plugin itself: Zoom Control, Zoom Output Manager,
Zoom Diagnostics, Zoom ISO Recorder, normal OBS scenes/sources, and TCP/OSC
automation.

A standalone engine hosts the Zoom SDK out of process and the OBS plugin talks
to it over IPC. The companion Sidecar app remains packaged for development, but
the in-plugin launcher is hidden until scene/look design can reliably create
and control OBS scenes with visual parity.

---

## Recently Shipped

Highlights from the last cycle, for context:

- **Audience audio routing** - a per-source mode that plays the residual active
  speaker after named isolated sources have claimed specific participants.
- **Zoom Diagnostics dock** - feed status, resolution, health reasons, recovery
  triggers, stale-feed auto-recovery, and support snapshot export.
- **Dockable Zoom Output Manager** - runtime source assignment, profiles,
  duplicate assignment warnings, screen-share assignment labels, and output
  health states.
- **Zoom ISO Recorder dock** - FFmpeg-backed participant ISO recording,
  optional OBS program recording, disk-free-space status, encoder state,
  duration, frame/audio counts, and file-size visibility.
- **HD video opt-in** - resolution-error logging at the SDK boundary so
  account-tier downgrades surface in the log instead of silently reducing
  quality.
- **Active Speaker Director** - Responsive / Balanced / Stable Panel presets,
  sensitivity, hold time, manual supersede, participant exclusions, and a
  dedicated CoreVideo Active Speaker source.
- **Sidecar launcher hidden** - the app still ships in release packages for
  development, but the OBS plugin no longer presents it as the supported
  production control surface.

---

## Now - within the next one or two releases

Things in flight or queued, scoped tightly enough that the shape is clear.

### Output stability hardening
**Problem:** Operators need a predictable answer when a participant feed does
not match the requested output. Color bars, stale frames, missing participants,
duplicate assignments, and lower-than-requested resolution should be obvious.

**Plan:** Continue tightening health states, retry thresholds, diagnostics
export, and log messages so test sessions produce actionable evidence without
guesswork.

### High-quality feed negotiation
**Problem:** Accounts with High Bandwidth Mode and the right app entitlement
should be able to pull more than two useful feeds, but Zoom can still negotiate
lower quality based on meeting state, bandwidth, participant camera state, or
SDK response.

**Plan:** Add more explicit SDK response logging, expose requested versus
observed quality in all operator surfaces, and keep automatic quality retries
bounded so sources at 1080p are not churned.

### Active Speaker production workflow
**Problem:** Speaker following is a production decision, not just a raw Zoom
event. Host/question-reader workflows need exclusions, smoother cuts, and clear
operator visibility.

**Plan:** Expand director state visibility, presets, manual supersede controls,
and exclusion handling on the dedicated CoreVideo Active Speaker source.

### Companion action parity
**Problem:** Companion controls Zoom join/leave and some assignments, but it
does not yet expose every production control available in the OBS docks.

**Plan:** Add actions and feedbacks for output health, active-speaker presets,
ISO recording state, diagnostics export, and audio routing modes.

### Documentation parity
**Problem:** Public docs should match what the current OBS plugin can actually
do and should not imply that sidecar scenes/looks are production-ready.

**Plan:** Keep GitHub docs and the published site aligned with each release.
Sidecar content should be marked as roadmap work until the OBS scene-control
contract is reliable.

---

## Next - coming months

Shape is clear, scope is bigger or depends on the Now items landing.

### Sidecar scene/look rebuild
**Problem:** The sidecar should eventually design looks and create matching OBS
scenes, but the previous implementation did not maintain reliable parity between
the sidecar canvas and OBS.

**Plan:** Treat OBS as the source of truth. Define a render plan that maps each
look to OBS scenes, nested slot scenes, participant sources, overlays,
backgrounds, ordering, opacity, shadows, and rounded corners. Audit after every
apply and repair drift before showing a look as ready.

### OBS Studio Mode bridge
**Problem:** A future sidecar PGM/PVW flow must not fight OBS Studio Mode.

**Plan:** Detect Studio Mode on connect; when active, route sidecar TAKE to
`TriggerStudioModeTransition`, mirror OBS preview/program back into PVW/PGM,
and surface a toggle to disable the bridge.

### Tally and transition visibility
**Problem:** Operators need to know which participants are on air, previewed,
recording, or stale.

**Plan:** Add slot-level tally, transition progress, and external feedback
events through the control server and Companion.

### Sidecar telemetry and health
**Problem:** If the sidecar returns to the operator workflow, it must show the
same feed state as the OBS docks.

**Plan:** Mirror frame rate, last-frame age, health reason, recovery attempts,
ISO state, and diagnostics export from the plugin/control API.

### Webinar audience handling
**Problem:** Webinars join correctly, but participant panels need clearer
separation between panelists, attendees, and broadcast-eligible sources.

**Plan:** Surface attendee/panelist roles from the SDK, filter the roster, and
limit assignments to roles that can produce media.

---

## Later - exploratory directions

Things we've thought about but haven't committed to. If any of these matters for
your workflow, tell us - that's how it moves up.

### Native compositor path for overlays
Right now overlays reach OBS as separate sources. A future path could composite
them directly into participant frames, eliminating scene-graph churn and giving
exact pixel-level parity between any design surface and the air feed.

### Multi-meeting / multi-room studio
Single-meeting today. A multi-room studio would need a meeting picker,
per-meeting subscriptions, and a routing layer that maps meetings to output
groups.

### Look marketplace
Looks can be JSON-portable once the OBS render contract is stable. A bundle
format and a way to share, install, and update look packs would turn the gallery
into a community library.

### Mobile control surface
A future network control surface could let a producer monitor or drive the show
from another device on the LAN.

### Multi-operator with roles
Today every operator on the control server has full authority. Roles such as
director, audio operator, and observer would unlock larger productions where
responsibilities are split.

### Large-meeting capacity guidance
Templates currently target up to eight outputs. Beyond that, the Zoom SDK raw
data callback patterns need real stress tests and a documented capacity envelope.

---

## Non-goals

- **Replacing OBS.** CoreVideo is a plugin and control layer for OBS. It is not
  intended to ship a parallel streaming engine.
- **Other conferencing platforms.** The product is purpose-built around the
  Zoom Meeting SDK. Adding Teams or Google Meet would be a separate project.
- **Browser-only operation.** The production path is native OBS integration. A
  browser surface might ride alongside but should not replace OBS.
- **Hosting Zoom features that are not in the SDK.** If the SDK does not expose
  a feed or mode, CoreVideo cannot invent it. We file enhancement requests with
  Zoom for gaps we hit.

---

## How this gets influenced

- **File an issue** describing the problem you're trying to solve, not just the
  feature you think you want.
- **Open a discussion** for anything in the Later section you want to see
  promoted. Concrete production examples carry weight.
- **Send a PR.** Plugin features live in `src/`, engine code in `engine/src/`,
  sidecar features in `sidecar/src/`, and Companion verbs in
  `companion/companion-module-corevideo-obs/`.

Last updated: see the commit log on this file.
