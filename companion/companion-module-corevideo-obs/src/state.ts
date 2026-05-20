// ── Zoom / CoreVideo plugin ──────────────────────────────────────────────────

export type MeetingState = 'idle' | 'joining' | 'in_meeting' | 'leaving' | 'recovering' | 'failed'

export interface Participant {
	id: number
	name: string
	has_video: boolean
	is_talking: boolean
	is_muted: boolean
}

export interface Output {
	source: string
	display_name: string
	participant_id: number
	active_speaker: boolean
	assignment_mode: 'participant' | 'active_speaker' | 'spotlight' | 'screen_share'
	spotlight_slot: number
	isolate_audio: boolean
	audience_audio?: boolean
	audio_channels: 'mono' | 'stereo'
	video_resolution?: '360p' | '720p' | '1080p'
	observed_width?: number
	observed_height?: number
	observed_fps?: number
	health_reason?: string
	health_label?: string
	signal_below_requested?: boolean
	signal_missing_or_stale?: boolean
	duplicate_participant_assignment?: boolean
}

export type ShowPhase = 'pre_show' | 'live' | 'post_show'

export interface SpeakerDirectorState {
	directed_speaker_id: number
	raw_speaker_id: number
	candidate_speaker_id: number
	last_speaker_id: number
	manual_speaker_id: number
	manual_active: boolean
	sensitivity_ms: number
	hold_ms: number
	require_video: boolean
	excluded_participant_ids: number[]
}

export interface IsoRecordingState {
	active: boolean
	sessionCount: number
}

// ── Combined module state ────────────────────────────────────────────────────

export interface ModuleState {
	zoom: {
		meetingState: MeetingState
		activeSpeakerId: number
		activeSpeakerName: string
		participants: Participant[]
		outputs: Output[]
		speakerDirector: SpeakerDirectorState
		isoRecording: IsoRecordingState
	}
	obs: {
		connected: boolean
		currentScene: string
		scenes: string[]
		recording: boolean
		recordingPaused: boolean
		streaming: boolean
		virtualCam: boolean
	}
	sidecar: {
		connected: boolean
		phase: ShowPhase
		templateId: string
		templateName: string
		templates: Array<{ id: string; name: string }>
		obsState: string
		currentScene: string
		scenes: string[]
	}
}

export function defaultState(): ModuleState {
	return {
		zoom: {
			meetingState: 'idle',
			activeSpeakerId: 0,
			activeSpeakerName: '',
			participants: [],
			outputs: [],
			speakerDirector: {
				directed_speaker_id: 0,
				raw_speaker_id: 0,
				candidate_speaker_id: 0,
				last_speaker_id: 0,
				manual_speaker_id: 0,
				manual_active: false,
				sensitivity_ms: 500,
				hold_ms: 2000,
				require_video: true,
				excluded_participant_ids: [],
			},
			isoRecording: {
				active: false,
				sessionCount: 0,
			},
		},
		obs: {
			connected: false,
			currentScene: '',
			scenes: [],
			recording: false,
			recordingPaused: false,
			streaming: false,
			virtualCam: false,
		},
		sidecar: {
			connected: false,
			phase: 'pre_show',
			templateId: '',
			templateName: '',
			templates: [],
			obsState: 'disconnected',
			currentScene: '',
			scenes: [],
		},
	}
}
