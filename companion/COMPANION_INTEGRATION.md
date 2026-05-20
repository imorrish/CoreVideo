# CoreVideo x Bitfocus Companion Integration

CoreVideo exposes local control APIs that Companion can drive.

| Protocol | Port | Best for |
|---|---:|---|
| TCP JSON | 19870 | Custom module, scripting, full push events |
| UDP OSC | 19871 | Companion's built-in OSC module, TouchOSC, quick prototyping |

## Custom Companion Module

The `companion-module-corevideo-obs/` directory is a Companion module written in
TypeScript. It talks directly to the CoreVideo OBS plugin over TCP JSON and can
optionally connect to OBS WebSocket and the Sidecar app.

### Actions

| Action | Description |
|---|---|
| Zoom: Join Meeting | Join by meeting ID, Zoom URL, or passcode |
| Zoom: Leave Meeting | Leave the current meeting |
| Zoom: Start/Stop Raw Media Engine | Start or stop raw media requests after joining |
| Zoom: Assign Participant to Output | Route a participant or active speaker to an OBS source |
| Zoom: Assign Spotlight Slot to Output | Track Zoom spotlight slot N |
| Zoom: Assign Screen Share to Output | Route active screen share |
| Zoom: Recover Stale Outputs | Force recovery retry for stale outputs |
| Zoom: Retry Low-Quality Outputs | Force quality retry for outputs below requested resolution |
| Zoom: Active Speaker Preset | Apply Responsive, Balanced, Stable Panel, or custom timing |
| Zoom: Active Speaker Manual Take/Release | Hold a participant as directed speaker, then return to automatic |
| Zoom: ISO Recording Start/Stop/Refresh Status | Control and query CoreVideo ISO recording |
| OBS controls | Scene switching, recording, streaming, virtual camera, source visibility |

Participant assignment supports **Mix**, **Isolated Participant**, and
**Audience / Residual** audio routing through `assign_output_ex`.

### Feedbacks

| Feedback | Description |
|---|---|
| Zoom: Meeting State | True when the Zoom meeting is in the selected state |
| Zoom: Meeting State Color | Button color tracks the state automatically |
| Zoom: Is Active Speaker | True when participant ID is active speaker |
| Zoom: Output Has Participant | True when a source has someone assigned |
| Zoom: Output Health Reason | True when a source has the selected health reason |
| Zoom: Output Needs Attention | True when a source is stale, missing, duplicate, or below requested quality |
| Zoom: Auto-Recovery Active | True during reconnect attempts |
| Zoom: ISO Recording Active | True while ISO recording is active |
| Zoom: Active Speaker Manual Take Active | True while manual speaker hold is active |

### Variables

| Variable | Value |
|---|---|
| `$(corevideo-obs:zoom_meeting_state)` | `idle`, `joining`, `in_meeting`, `leaving`, `recovering`, or `failed` |
| `$(corevideo-obs:zoom_active_speaker_name)` | Display name of current active speaker |
| `$(corevideo-obs:zoom_active_speaker_id)` | Numeric user ID |
| `$(corevideo-obs:zoom_directed_speaker_id)` | Active Speaker Director's chosen participant |
| `$(corevideo-obs:zoom_speaker_preset)` | Responsive, Balanced, Stable Panel, or Custom |
| `$(corevideo-obs:zoom_iso_recording)` | `yes` or `no` |
| `$(corevideo-obs:zoom_iso_session_count)` | Active ISO session count |
| `$(corevideo-obs:zoom_output_N_source)` | OBS source name for output N |
| `$(corevideo-obs:zoom_output_N_participant)` | Participant name assigned to output N |
| `$(corevideo-obs:zoom_output_N_mode)` | Assignment mode for output N |
| `$(corevideo-obs:zoom_output_N_health)` | Output health label |
| `$(corevideo-obs:zoom_output_N_resolution)` | Observed resolution or `No signal` |
| `$(corevideo-obs:zoom_output_N_fps)` | Observed FPS |

### Development Build

```bash
cd companion/companion-module-corevideo-obs
npm install
npm run build
```

Then add the `dist/` folder as a local module in Companion Developer Mode.

The module is pinned to `@companion-module/base` `^1.13.6`, matching the current
Companion v3-style module API used by this source tree.

## TCP JSON API

All requests are newline-delimited JSON sent over a persistent TCP connection to
`127.0.0.1:19870`. Include `"token": "..."` in every request if a control token
is configured.

On connect, the module sends:

```json
{"cmd":"subscribe_events"}
{"cmd":"status"}
{"cmd":"list_outputs"}
{"cmd":"list_participants"}
{"cmd":"iso_recording_status"}
```

CoreVideo pushes events as they occur:

```json
{"event":"meeting_state","state":"in_meeting"}
{"event":"active_speaker","user_id":16778240,"name":"Sam Chen"}
{"event":"roster_changed"}
{"event":"output_changed"}
```

Common commands:

| `cmd` | Key fields | Notes |
|---|---|---|
| `status` | none | Returns meeting state, active speaker, speaker director, recovery |
| `list_participants` | none | Returns participant roster |
| `list_outputs` | none | Returns output assignments and health states |
| `join` | `meeting_id`, `passcode`, `display_name` | Accepts URL or numeric ID |
| `leave` | none | Leave meeting |
| `start_engine` / `stop_engine` | none | Start/stop raw media requests |
| `assign_output_ex` | `source`, `mode`, `participant_id`, `spotlight_slot`, `isolate_audio`, `audience_audio`, `audio_channels`, `video_resolution` | Full output routing |
| `recover_stale_outputs` | `force` | Retry stale outputs |
| `upgrade_low_quality_outputs` | `force` | Retry outputs below requested resolution |
| `speaker_director_status` | none | Query director state |
| `speaker_director_configure` | `sensitivity_ms`, `hold_ms`, `require_video`, `excluded_participant_ids` | Configure active speaker logic |
| `speaker_director_take` | `participant_id` | Manual speaker hold |
| `speaker_director_release` | none | Return to automatic speaker direction |
| `iso_recording_start` | `output_dir`, `ffmpeg_path`, `record_program` | Start ISO recording |
| `iso_recording_stop` | none | Stop ISO recording |
| `iso_recording_status` | none | Query ISO recording state |
| `recovery_cancel` | none | Cancel active reconnect |
| `oauth_callback` | `url` | Internal OAuth redirect handler |

## OSC Notes

The OSC API remains useful for quick button mapping, but the custom Companion
module should be preferred for production because it receives structured output
health, speaker director, ISO, and recovery state over TCP.

Both servers bind to `127.0.0.1` by default. For remote Companion machines, use
a secure port-forwarding setup until remote binding is explicitly supported in
CoreVideo settings.
