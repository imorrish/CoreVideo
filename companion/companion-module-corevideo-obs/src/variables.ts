import type { CompanionVariableDefinitions, CompanionVariableValues } from '@companion-module/base'
import type { ModuleState } from './state.js'

export const variableDefinitions: CompanionVariableDefinitions = {
	// ── Zoom ──────────────────────────────────────────────────────────────────
	zoom_meeting_state:       { name: 'Zoom: Meeting State' },
	zoom_active_speaker_name: { name: 'Zoom: Active Speaker Name' },
	zoom_active_speaker_id:   { name: 'Zoom: Active Speaker ID' },
	zoom_participant_count:   { name: 'Zoom: Participant Count' },
	zoom_output_count:        { name: 'Zoom: Output Count' },
	// ── OBS ───────────────────────────────────────────────────────────────────
	obs_current_scene: { name: 'OBS: Current Scene' },
	obs_recording:     { name: 'OBS: Recording' },
	obs_streaming:     { name: 'OBS: Streaming' },
	obs_virtual_cam:   { name: 'OBS: Virtual Camera Active' },
	// ── Sidecar / Show ────────────────────────────────────────────────────────
	sidecar_phase:         { name: 'Show: Phase' },
	sidecar_template_id:   { name: 'Show: Template ID' },
	sidecar_template_name: { name: 'Show: Template Name' },
	sidecar_scene:         { name: 'Show: Current Scene' },
}

export function buildVariableValues(state: ModuleState): CompanionVariableValues {
	const vals: CompanionVariableValues = {
		// Zoom
		zoom_meeting_state:       state.zoom.meetingState,
		zoom_active_speaker_name: state.zoom.activeSpeakerName,
		zoom_active_speaker_id:   String(state.zoom.activeSpeakerId),
		zoom_participant_count:   String(state.zoom.participants.length),
		zoom_output_count:        String(state.zoom.outputs.length),
		// OBS
		obs_current_scene: state.obs.currentScene,
		obs_recording:     state.obs.recording ? 'yes' : 'no',
		obs_streaming:     state.obs.streaming ? 'yes' : 'no',
		obs_virtual_cam:   state.obs.virtualCam ? 'yes' : 'no',
		// Sidecar
		sidecar_phase:         state.sidecar.phase,
		sidecar_template_id:   state.sidecar.templateId,
		sidecar_template_name: state.sidecar.templateName,
		sidecar_scene:         state.sidecar.currentScene,
	}

	state.zoom.outputs.forEach((o, i) => {
		const n = i + 1
		vals[`zoom_output_${n}_source`]      = o.source
		vals[`zoom_output_${n}_participant`]  = o.display_name || String(o.participant_id)
		vals[`zoom_output_${n}_mode`]         = o.assignment_mode
	})

	return vals
}

export function buildOutputVariableDefs(count: number): CompanionVariableDefinitions {
	const defs: CompanionVariableDefinitions = {}
	for (let i = 1; i <= Math.min(count, 16); i++) {
		defs[`zoom_output_${i}_source`]      = { name: `Zoom Output ${i} Source` }
		defs[`zoom_output_${i}_participant`] = { name: `Zoom Output ${i} Participant` }
		defs[`zoom_output_${i}_mode`]        = { name: `Zoom Output ${i} Assignment Mode` }
	}
	return defs
}
