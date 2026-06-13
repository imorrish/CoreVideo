import { describe, expect, it } from 'vitest'
import { defaultState } from './state.js'
import { buildVariableValues } from './variables.js'

describe('defaultState', () => {
	it('starts in a clean, disconnected idle state', () => {
		const s = defaultState()
		expect(s.zoom.meetingState).toBe('idle')
		expect(s.zoom.participants).toEqual([])
		expect(s.zoom.outputs).toEqual([])
		expect(s.obs.connected).toBe(false)
		expect(s.sidecar.connected).toBe(false)
		expect(s.sidecar.phase).toBe('pre_show')
	})

	it('returns a fresh object each call (no shared references)', () => {
		const a = defaultState()
		const b = defaultState()
		a.zoom.participants.push({
			id: 1,
			name: 'A',
			has_video: true,
			is_talking: false,
			is_muted: false,
		})
		expect(b.zoom.participants).toEqual([])
	})
})

describe('buildVariableValues', () => {
	it('maps booleans to yes/no and counts collections', () => {
		const s = defaultState()
		s.zoom.activeSpeakerName = 'Alice'
		s.zoom.activeSpeakerId = 42
		s.zoom.participants = [
			{ id: 1, name: 'Alice', has_video: true, is_talking: true, is_muted: false },
			{ id: 2, name: 'Bob', has_video: false, is_talking: false, is_muted: true },
		]
		s.obs.recording = true
		s.obs.streaming = false

		const v = buildVariableValues(s)
		expect(v.zoom_active_speaker_name).toBe('Alice')
		expect(v.zoom_active_speaker_id).toBe('42')
		expect(v.zoom_participant_count).toBe('2')
		expect(v.obs_recording).toBe('yes')
		expect(v.obs_streaming).toBe('no')
	})

	it('emits per-output variables, falling back to participant id when no name', () => {
		const s = defaultState()
		s.zoom.outputs = [
			{
				source: 'cam1',
				display_name: 'Speaker One',
				participant_id: 7,
				active_speaker: true,
				assignment_mode: 'active_speaker',
				spotlight_slot: 0,
				isolate_audio: false,
				audio_channels: 'stereo',
			},
			{
				source: 'cam2',
				display_name: '',
				participant_id: 9,
				active_speaker: false,
				assignment_mode: 'participant',
				spotlight_slot: 0,
				isolate_audio: false,
				audio_channels: 'mono',
			},
		]

		const v = buildVariableValues(s)
		expect(v.zoom_output_count).toBe('2')
		expect(v.zoom_output_1_source).toBe('cam1')
		expect(v.zoom_output_1_participant).toBe('Speaker One')
		expect(v.zoom_output_1_mode).toBe('active_speaker')
		// display_name empty -> falls back to the participant id as string
		expect(v.zoom_output_2_participant).toBe('9')
	})
})
