import type { CompanionActionDefinitions } from '@companion-module/base'
import type { CoreVideoInstance } from './index.js'

function numberOption(value: unknown, fallback = 0): number {
	const n = typeof value === 'number' ? value : Number(value)
	return Number.isFinite(n) ? n : fallback
}

function speakerPresetTiming(preset: unknown, customSensitivity: unknown, customHold: unknown): { sensitivity_ms: number; hold_ms: number } {
	switch (preset) {
		case 'responsive':
			return { sensitivity_ms: 250, hold_ms: 1200 }
		case 'stable':
			return { sensitivity_ms: 900, hold_ms: 3500 }
		case 'custom':
			return {
				sensitivity_ms: numberOption(customSensitivity, 500),
				hold_ms: numberOption(customHold, 2000),
			}
		case 'balanced':
		default:
			return { sensitivity_ms: 500, hold_ms: 2000 }
	}
}

function audioOptions(route: unknown): { isolate_audio: boolean; audience_audio?: boolean } {
	switch (route) {
		case 'iso':
			return { isolate_audio: true, audience_audio: false }
		case 'audience':
			return { isolate_audio: false, audience_audio: true }
		case 'mix':
		default:
			return { isolate_audio: false, audience_audio: false }
	}
}

export function buildActions(inst: CoreVideoInstance): CompanionActionDefinitions {
	return {

		// ── Zoom: Meeting ───────────────────────────────────────────────────────

		zoom_join: {
			name: 'Zoom: Join Meeting',
			options: [
				{ type: 'textinput', id: 'meeting_id',   label: 'Meeting ID or Zoom URL', default: '' },
				{ type: 'textinput', id: 'passcode',     label: 'Passcode (optional)',    default: '' },
				{ type: 'textinput', id: 'display_name', label: 'Display Name',           default: 'OBS' },
			],
			callback: (a) => inst.sendPlugin({ cmd: 'join', meeting_id: a.options.meeting_id,
				passcode: a.options.passcode, display_name: a.options.display_name }),
		},

		zoom_leave: {
			name: 'Zoom: Leave Meeting',
			options: [],
			callback: () => inst.sendPlugin({ cmd: 'leave' }),
		},

		zoom_assign: {
			name: 'Zoom: Assign Participant to Output',
			options: [
				{ type: 'textinput', id: 'source',         label: 'OBS Source Name',                 default: '' },
				{ type: 'number',    id: 'participant_id', label: 'Participant ID (0 = active spkr)', default: 0, min: 0, max: 999999999 },
				{ type: 'checkbox',  id: 'active_speaker', label: 'Track Active Speaker',             default: false },
				{
					type: 'dropdown', id: 'audio_route', label: 'Audio Route', default: 'mix',
					choices: [
						{ id: 'mix', label: 'Mix' },
						{ id: 'iso', label: 'Isolated Participant' },
						{ id: 'audience', label: 'Audience / Residual' },
					],
				},
			],
			callback: (a) => inst.sendPlugin({
				cmd: 'assign_output_ex',
				source: a.options.source,
				mode: a.options.active_speaker ? 'active_speaker' : 'participant',
				participant_id: a.options.participant_id,
				...audioOptions(a.options.audio_route),
			}),
		},

		zoom_assign_spotlight: {
			name: 'Zoom: Assign Spotlight Slot to Output',
			options: [
				{ type: 'textinput', id: 'source', label: 'OBS Source Name',        default: '' },
				{ type: 'number',    id: 'slot',   label: 'Spotlight Slot (1-based)', default: 1, min: 1, max: 49 },
			],
			callback: (a) => inst.sendPlugin({ cmd: 'assign_output_ex', source: a.options.source,
				mode: 'spotlight', spotlight_slot: a.options.slot }),
		},

		zoom_assign_screen_share: {
			name: 'Zoom: Assign Screen Share to Output',
			options: [
				{ type: 'textinput', id: 'source', label: 'OBS Source Name', default: '' },
			],
			callback: (a) => inst.sendPlugin({ cmd: 'assign_output_ex',
				source: a.options.source, mode: 'screen_share' }),
		},

		zoom_cancel_recovery: {
			name: 'Zoom: Cancel Auto-Recovery',
			options: [],
			callback: () => inst.sendPlugin({ cmd: 'recovery_cancel' }),
		},

		zoom_start_engine: {
			name: 'Zoom: Start Raw Media Engine',
			options: [],
			callback: () => inst.sendPlugin({ cmd: 'start_engine' }),
		},

		zoom_stop_engine: {
			name: 'Zoom: Stop Raw Media Engine',
			options: [],
			callback: () => inst.sendPlugin({ cmd: 'stop_engine' }),
		},

		zoom_recover_stale_outputs: {
			name: 'Zoom: Recover Stale Outputs',
			options: [
				{ type: 'checkbox', id: 'force', label: 'Bypass Cooldown', default: true },
			],
			callback: (a) => inst.sendPlugin({ cmd: 'recover_stale_outputs', force: a.options.force }),
		},

		zoom_upgrade_low_quality_outputs: {
			name: 'Zoom: Retry Low-Quality Outputs',
			options: [
				{ type: 'checkbox', id: 'force', label: 'Bypass Cooldown', default: true },
			],
			callback: (a) => inst.sendPlugin({ cmd: 'upgrade_low_quality_outputs', force: a.options.force }),
		},

		zoom_speaker_preset: {
			name: 'Zoom: Active Speaker Preset',
			options: [
				{
					type: 'dropdown', id: 'preset', label: 'Preset', default: 'balanced',
					choices: [
						{ id: 'responsive', label: 'Responsive' },
						{ id: 'balanced', label: 'Balanced' },
						{ id: 'stable', label: 'Stable Panel' },
						{ id: 'custom', label: 'Custom' },
					],
				},
				{ type: 'number', id: 'sensitivity_ms', label: 'Custom Sensitivity ms', default: 500, min: 0, max: 5000 },
				{ type: 'number', id: 'hold_ms', label: 'Custom Hold ms', default: 2000, min: 0, max: 10000 },
				{ type: 'checkbox', id: 'require_video', label: 'Require Video', default: true },
				{ type: 'number', id: 'exclude_1', label: 'Exclude Participant 1', default: 0, min: 0, max: 999999999 },
				{ type: 'number', id: 'exclude_2', label: 'Exclude Participant 2', default: 0, min: 0, max: 999999999 },
			],
			callback: (a) => {
				const timing = speakerPresetTiming(a.options.preset, a.options.sensitivity_ms, a.options.hold_ms)
				const excluded = [numberOption(a.options.exclude_1), numberOption(a.options.exclude_2)].filter((id) => id > 0)
				inst.sendPlugin({
					cmd: 'speaker_director_configure',
					...timing,
					require_video: a.options.require_video !== false,
					excluded_participant_ids: excluded,
				})
			},
		},

		zoom_speaker_take: {
			name: 'Zoom: Active Speaker Manual Take',
			options: [
				{ type: 'number', id: 'participant_id', label: 'Participant ID', default: 0, min: 0, max: 999999999 },
			],
			callback: (a) => inst.sendPlugin({
				cmd: 'speaker_director_take',
				participant_id: a.options.participant_id,
			}),
		},

		zoom_speaker_release: {
			name: 'Zoom: Active Speaker Release Manual Take',
			options: [],
			callback: () => inst.sendPlugin({ cmd: 'speaker_director_release' }),
		},

		zoom_iso_start: {
			name: 'Zoom: ISO Recording Start',
			options: [
				{ type: 'textinput', id: 'output_dir', label: 'Output Folder', default: '' },
				{ type: 'textinput', id: 'ffmpeg_path', label: 'FFmpeg Path', default: 'ffmpeg' },
				{ type: 'checkbox', id: 'record_program', label: 'Also Record OBS Program', default: true },
			],
			callback: (a) => inst.sendPlugin({
				cmd: 'iso_recording_start',
				output_dir: a.options.output_dir,
				ffmpeg_path: a.options.ffmpeg_path,
				record_program: a.options.record_program,
			}),
		},

		zoom_iso_stop: {
			name: 'Zoom: ISO Recording Stop',
			options: [],
			callback: () => inst.sendPlugin({ cmd: 'iso_recording_stop' }),
		},

		zoom_iso_refresh_status: {
			name: 'Zoom: ISO Recording Refresh Status',
			options: [],
			callback: () => inst.sendPlugin({ cmd: 'iso_recording_status' }),
		},

		// ── OBS: Scenes ─────────────────────────────────────────────────────────

		obs_switch_scene: {
			name: 'OBS: Switch Scene',
			options: [
				{ type: 'textinput', id: 'scene', label: 'Scene Name', default: '' },
			],
			callback: (a) => inst.obsRequest('SetCurrentProgramScene',
				{ sceneName: a.options.scene }),
		},

		obs_switch_scene_pick: {
			name: 'OBS: Switch Scene (dropdown)',
			options: [
				{
					type: 'dropdown', id: 'scene', label: 'Scene',
					default: '', allowCustom: true,
					choices: inst.state.obs.scenes.map((s) => ({ id: s, label: s })),
				},
			],
			callback: (a) => inst.obsRequest('SetCurrentProgramScene',
				{ sceneName: a.options.scene }),
		},

		// ── OBS: Recording ──────────────────────────────────────────────────────

		obs_start_record: {
			name: 'OBS: Start Recording',
			options: [],
			callback: () => inst.obsRequest('StartRecord'),
		},

		obs_stop_record: {
			name: 'OBS: Stop Recording',
			options: [],
			callback: () => inst.obsRequest('StopRecord'),
		},

		obs_toggle_record: {
			name: 'OBS: Toggle Recording',
			options: [],
			callback: () => inst.obsRequest('ToggleRecord'),
		},

		obs_pause_record: {
			name: 'OBS: Pause / Resume Recording',
			options: [],
			callback: () => inst.obsRequest('ToggleRecordPause'),
		},

		// ── OBS: Streaming ──────────────────────────────────────────────────────

		obs_start_stream: {
			name: 'OBS: Start Streaming',
			options: [],
			callback: () => inst.obsRequest('StartStream'),
		},

		obs_stop_stream: {
			name: 'OBS: Stop Streaming',
			options: [],
			callback: () => inst.obsRequest('StopStream'),
		},

		obs_toggle_stream: {
			name: 'OBS: Toggle Streaming',
			options: [],
			callback: () => inst.obsRequest('ToggleStream'),
		},

		// ── OBS: Virtual Camera ─────────────────────────────────────────────────

		obs_start_vcam: {
			name: 'OBS: Start Virtual Camera',
			options: [],
			callback: () => inst.obsRequest('StartVirtualCam'),
		},

		obs_stop_vcam: {
			name: 'OBS: Stop Virtual Camera',
			options: [],
			callback: () => inst.obsRequest('StopVirtualCam'),
		},

		obs_toggle_vcam: {
			name: 'OBS: Toggle Virtual Camera',
			options: [],
			callback: () => inst.obsRequest('ToggleVirtualCam'),
		},

		// ── OBS: Source visibility ──────────────────────────────────────────────

		obs_source_visible: {
			name: 'OBS: Set Source Visibility',
			options: [
				{ type: 'textinput', id: 'scene',   label: 'Scene Name',  default: '' },
				{ type: 'textinput', id: 'source',  label: 'Source Name', default: '' },
				{
					type: 'dropdown', id: 'enabled', label: 'Visibility',
					default: 'true',
					choices: [{ id: 'true', label: 'Visible' }, { id: 'false', label: 'Hidden' }],
				},
			],
			callback: async (a) => {
				const scene  = a.options.scene  as string || inst.state.obs.currentScene
				const source = a.options.source as string
				// Look up sceneItemId first
				const data = await inst.obsRequest<{ sceneItems: Array<{ sceneItemId: number; sourceName: string }> }>(
					'GetSceneItemList', { sceneName: scene })
				const item = data?.sceneItems?.find((i) => i.sourceName === source)
				if (!item) { inst.log('warn', `OBS source "${source}" not found in scene "${scene}"`); return }
				inst.obsRequest('SetSceneItemEnabled', {
					sceneName: scene, sceneItemId: item.sceneItemId,
					sceneItemEnabled: a.options.enabled === 'true',
				})
			},
		},

		// ── Show: Phases (Sidecar) ──────────────────────────────────────────────

		show_set_phase: {
			name: 'Show: Set Phase',
			options: [
				{
					type: 'dropdown', id: 'phase', label: 'Phase', default: 'live',
					choices: [
						{ id: 'pre_show',  label: 'Pre-Show' },
						{ id: 'live',      label: '● LIVE'   },
						{ id: 'post_show', label: 'Post-Show' },
					],
				},
			],
			callback: (a) => inst.sendSidecar({ cmd: 'set_phase', phase: a.options.phase }),
		},

		show_apply_template: {
			name: 'Show: Apply Layout Template',
			options: [
				{
					type: 'dropdown', id: 'template_id', label: 'Template',
					default: '4-up-grid', allowCustom: true,
					choices: inst.state.sidecar.templates.map((t) => ({ id: t.id, label: t.name })),
				},
			],
			callback: (a) => inst.sendSidecar({ cmd: 'apply_template',
				template_id: a.options.template_id }),
		},

		show_set_scene: {
			name: 'Show: Switch OBS Scene (via Sidecar)',
			options: [
				{
					type: 'dropdown', id: 'scene', label: 'Scene', default: '', allowCustom: true,
					choices: inst.state.sidecar.scenes.map((s) => ({ id: s, label: s })),
				},
			],
			callback: (a) => inst.sendSidecar({ cmd: 'set_scene', scene: a.options.scene }),
		},
	}
}
