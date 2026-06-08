# CoreVideo Validation Matrix

Use this matrix to close the non-auth release gaps. It intentionally avoids
changing the Zoom sign-in or meeting-join path.

## Screen Share Live Validation

- Assign one output to **Screen share** before anyone shares.
- Confirm Output Manager reports `Screen share unavailable`.
- Start a screen share from a meeting participant.
- Confirm the output moves through waiting-for-frame, then OK or lower-quality
  health depending on the delivered feed.
- Stop the share.
- Confirm the output returns to `Screen share unavailable` without changing OBS
  source geometry.
- Export a support bundle and confirm `screen_share_unavailable` appears only
  when no share is active.

## Active Speaker Director Live Validation

- Use at least three participants with video enabled.
- Confirm sensitivity prevents immediate cuts to short interruptions.
- Confirm hold time blocks rapid back-and-forth switching.
- Toggle **Require video** and verify audio-only talkers are ignored when it is
  enabled.
- Add an exclusion for a current speaker and verify the director moves to an
  eligible speaker.
- Use manual take/release and confirm release does not cut away unnecessarily
  when the manual speaker remains valid.

## OBS Lifecycle And Reopen

- Start OBS with the plugin installed.
- Open Zoom Control, Output Manager, Diagnostics, and ISO Recorder.
- Close and reopen each dock from **Tools**.
- Create a participant source, active-speaker source, screen-share source, and
  participant-audio source.
- Close OBS from the normal Exit menu.
- Reopen OBS and confirm the same sources load without crash dialogs.
- Validate lifecycle log markers:

```powershell
.\scripts\obs-scene-smoke-test.ps1 -LogOnly -ExpectDockShow -ExpectShutdown `
  -ObsLogPath "$env:APPDATA\obs-studio\logs\YYYY-MM-DD HH-MM-SS.txt" `
  -ExpectedDockId ZoomControlDock,ZoomOutputManagerDock,ZoomDiagnosticsDock,ZoomIsoRecorderDock
```

## Installer And Release Package

- Build or stage a package.
- Validate core runtime and optional full Zoom runtime:

```powershell
.\scripts\Test-CoreVideoPackage.ps1 -PackageRoot .\dist\CoreVideo-Windows-x64 `
  -FullRuntime -ManifestPath .\dist\CoreVideo-Windows-x64.manifest.json
```

- If sidecar is included, the validator also checks required template/look JSON.
- If secret env vars are present, the validator fails if their values appear in
  the plugin DLL.

## Automated OBS Smoke

Use this when OBS WebSocket is enabled:

```powershell
.\scripts\obs-scene-smoke-test.ps1 -VerifyCoreVideoPlugin -CreateCoreVideoInputs `
  -ParticipantCount 8
```

This confirms CoreVideo source kinds are registered and can be instantiated,
then creates an 8-slot scene plus a screen-share scene.

## Sidecar Gate

Sidecar remains a lower-priority production surface. Before presenting it as
release-ready:

- `ctest --test-dir build-vs-release -C Release --output-on-failure` must pass
  all sidecar tests.
- Packaged sidecar builds must include templates and looks validated by
  `Test-CoreVideoPackage.ps1`.
- Run an OBS WebSocket smoke pass against the sidecar-created scenes before
  publishing sidecar-facing release notes.
