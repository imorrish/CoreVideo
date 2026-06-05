# CoreVideo Roadmap

> Where we're heading. This roadmap is a living document — items shift as
> we learn from production use. Horizons are directional, not commitments.
> Everything here is subject to change.

CoreVideo is an OBS Studio plugin that pulls per-participant raw video,
audio, screen share, and interpretation channels out of a Zoom meeting via
the Zoom Meeting SDK — no screen capture, no virtual camera. The optional
**Sidecar** app is a Qt6 control-surface track for broadcast M/E workflows
(PGM / PVW, TAKE / AUTO / FTB), Looks, overlays, and per-slot audio routing;
those items should be treated as Sidecar-specific or roadmap capabilities
unless called out in the plugin docs. A standalone **engine** hosts the Zoom
SDK out of process; the OBS plugin talks to it over IPC, with Companion and
OSC integrations covering the automation paths described below.

The roadmap below is organized into **Now** (the next one or two releases),
**Next** (the following quarter), and **Later** (exploratory directions).
Every item is annotated with the user problem it's solving so it's easy to
tell whether something matters for your setup.

---

## Recently Shipped

Highlights from the last cycle, for context:

- **M/E switcher** — PGM/PVW dual canvas, TAKE (cut), AUTO (timed fade
  through black), FTB (fade to black), SWAP (off-air comparison).
- **Broadcast Looks** — template + theme + overlays + tile styling
  composed as a single saveable unit. Custom Looks persist to disk and
  merge with the built-in gallery at runtime.
- **Look Designer** — visual editor for slot geometry, tile styling, and
  backgrounds, with round-trip controls to OBS.
- **Audience audio routing** — a per-source mode that plays the residual
  active speaker (everyone not bound to an iso source). Pairs with
  isolated sources for "named talent + audience overflow" coverage.
- **Zoom diagnostics window** — feed status, resolution, recovery
  triggers, stale-feed auto-recovery.
- **OBS geometry audit** — drift detection, repair, and a Sync Inspector
  for verifying the scene graph matches the staged Look.
- **HD video opt-in** + resolution-error logging at the SDK boundary so
  account-tier downgrades surface in the log instead of silently
  reducing quality.
- **Active Speaker Director** — sensitivity, hold time, manual supersede,
  and a dedicated CoreVideo Active Speaker source.

---

## Now — within the next one or two releases

Things in flight or queued, scoped tightly enough that the shape is clear.

### Companion M/E verb coverage
**Problem:** Companion controls Zoom join/leave, OBS scenes, and template
apply, but not the sidecar's M/E flow. Remote control from a Stream Deck
can't drive TAKE / AUTO / FTB / SWAP today.

**Plan:** Extend the sidecar control server with `stage_look`, `take`,
`auto`, `ftb`, `swap`, and `fire_overlay` verbs, plus push events for
PGM / PVW changes and in-flight transitions. Ship matching Companion
actions and feedbacks.

### Audience audio in Companion
**Problem:** The Companion "Assign Participant to Output" action exposes
an `isolate_audio` checkbox but not the new audience routing — operators
have to dive into OBS source properties or the sidecar canvas.

**Plan:** Promote the iso checkbox to a tri-state (Mix / Iso / Audience),
matching the sidecar and the control-server protocol.

### Per-Look audio routing persistence
**Problem:** Slot routing (Mix / Iso / Audience) survives a TAKE but not
a sidecar restart, and saving a Look doesn't capture its routing intent.
Operators rebuild the configuration every session.

**Plan:** Add a slot-routing array to the Look JSON; save it with custom
Looks; restore on load so a Look carries its audio plan with it.

### macOS hardware acceleration parity
**Problem:** Windows release builds now ship with FFmpeg-backed HW accel
(CUDA / QSV), but macOS releases still fall back to the CPU even on
hardware that supports VideoToolbox.

**Plan:** Brew-install FFmpeg in the macOS CI job, enable
`ENABLE_FFMPEG_HW_ACCEL`, and bundle the LGPL dylibs into the plugin
package with rpath adjustments.

### Overlay completeness on air
**Problem:** Overlays render with full styling on the sidecar canvas, but
the OBS-side push uses basic `text_gdiplus` sources. Theme accent colors,
auto-out timing (`durationMs`), and animated entries don't reach the
broadcast.

**Plan:** Honor `durationMs` as a sidecar-driven auto-clear timer; map
theme accent to source text color; add the bug/title-card/ticker
variants as proper styled sources rather than text-only.

---

## Next — coming months

Shape is clear, scope is bigger or depends on the Now items landing.

### OBS Studio Mode bridge
**Problem:** Sidecar PGM/PVW mimics broadcast M/E but doesn't talk to
OBS's own Studio Mode. Operators running Studio Mode have two preview
buses fighting each other.

**Plan:** Detect Studio Mode on connect; when active, route sidecar TAKE
to `TriggerStudioModeTransition`, mirror OBS preview/program back into
PVW/PGM, and surface a toggle to disable the bridge for operators who
prefer the sidecar-only flow.

### ISO recording in the sidecar
**Problem:** The engine already runs per-participant ISO recording with
status JSON, but operators can't see recording state, bitrate, or disk
usage from the sidecar. Verification today means tailing engine logs.

**Plan:** New sidecar page subscribing to engine recording events:
per-output armed/recording state, bitrate, file size, free disk space,
and Companion verbs to arm/disarm.

### Tally + transition visibility
**Problem:** PGM and PVW canvases already imply tally via labels, but
there's no per-slot on-air indicator and no transition progress feedback
during AUTO.

**Plan:** Slot-level tally LED in the canvas; a thin progress bar across
the TAKE/AUTO bar during transitions; LED signal exported via the
control server so external hardware tally (Stream Deck, GPI) can react.

### Sidecar telemetry & health
**Problem:** When something degrades mid-show (a feed goes stale, a
subscription drops, frames stutter), the operator finds out from the
audience.

**Plan:** Per-source frame-rate, last-frame-age, and recovery-attempt
counters surfaced in the sidecar status bar; threshold alerts; an
exportable health snapshot for support tickets.

### Webinar audience handling
**Problem:** Webinars join correctly, but the participant panel mixes
panelists and webinar attendees, and there's no way to scope the roster
to broadcast-eligible participants.

**Plan:** Attendee / panelist role surfaced from the SDK; filter and
visual separation in the panel; assignment limited to broadcast-eligible
roles.

---

## Later — exploratory directions

Things we've thought about but haven't committed to. If any of these
matters for your workflow, tell us — that's how it moves up.

### Native compositor path for overlays
Right now overlays reach OBS as separate sources. A future path could
composite them directly into the participant frames, eliminating
scene-graph churn and giving exact pixel-level parity between the
sidecar canvas and the air feed.

### Multi-meeting / multi-room studio
Single-meeting today. A multi-room studio would need a meeting picker,
per-meeting subscriptions, and a routing layer that maps meetings to
output groups.

### Look marketplace
Looks are JSON-portable. A bundle format and a way to share, install,
and update Look packs would turn the gallery into a community library.

### Mobile control surface
The sidecar's control server is network-ready. A read-only mobile
viewer — and eventually a touch-deck control surface — would let a
producer drive the show from anywhere on the LAN.

### Multi-operator with roles
Today every operator on the control server has full authority. Roles
(director, audio op, observer) and per-verb authorization would unlock
larger productions where responsibilities are split.

### Large-meeting capacity guidance
Templates scale to eight slots out of the box. Beyond that, the Zoom
SDK's raw-data callback patterns deserve real stress tests and a
documented capacity envelope.

---

## Non-goals

A roadmap is also what we are *not* doing.

- **Replacing OBS.** CoreVideo is a plugin and a control surface for OBS.
  We don't intend to ship a parallel streaming engine.
- **Other conferencing platforms.** The product is purpose-built around
  the Zoom Meeting SDK. Adding Teams or Google Meet would be a different
  project, not a feature.
- **Browser-only operation.** The sidecar is a native Qt app on purpose
  — it shares clock and IPC with the OBS plugin. A browser surface might
  ride alongside but won't replace it.
- **Hosting Zoom features that aren't in the SDK.** If the SDK doesn't
  expose it (e.g., a 60-fps participant mode), we can't pull it in. We
  do file enhancement requests with Zoom for the gaps we hit.

---

## How this gets influenced

- **File an issue** describing the problem you're trying to solve — not
  the feature you think you want. The shape of the fix usually changes
  once the underlying need is clear.
- **Open a discussion** for anything in the *Later* section you want to
  see promoted. Concrete production examples carry weight.
- **Send a PR.** The codebase is structured to make most additions
  contained: sidecar features live in `sidecar/src/`, plugin features in
  `src/`, engine code in `engine/src/`, Companion verbs in
  `companion/companion-module-corevideo-obs/`.

Last updated: see the commit log on this file.
