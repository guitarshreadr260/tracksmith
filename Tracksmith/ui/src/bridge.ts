export type AutomationPoint={time:number;value:number};
export type AutomationTrack={track:number;mode:number;gain:number;panValue:number;volume?:AutomationPoint[];pan?:AutomationPoint[]};
export type PluginAutomationState={channel:number;slot:number;plugin?:string;selected?:string;parameters?:{id:string;name:string;value:number;text:string}[];lanes?:{id:string;name:string;read:boolean;mode?:number;points:AutomationPoint[]}[];canUndo?:boolean;canRedo?:boolean};
export type IOChannel={id:number;name:string;hardwareName:string;active:boolean};
export type IOState={deviceKey:string;sessionDeviceKey:string;matched:boolean;mainOutputPair:number;maxInputs:number;maxOutputs:number;maxRecordTracks:number;inputs:IOChannel[];outputs:IOChannel[]};
export type Command =
 | {type:'ioChannel';kind:'input'|'output';index:number;enabled:boolean}
 | {type:'ioName';kind:'input'|'output';index:number;name:string}
 | {type:'ioMainOutput';pair:number}
 | {type:'ioUseDevice'|'ioResetNames'}
 | {type:'ezAnalysePart'; neural?:boolean;referenceInfluence?:number;id:number;clip?:number}
 | {type:'ezApplyGroove'}
 | {type:'tempo';bpm:number}
 | {type:'shuttle';direction:-1|1}
 | {type:'createClickTrack'|'clickSettings'}
 | {type:'addEZInstrument';instrument:'guitar'|'drums'}
 | {type:'ezDrumHit';id:number;drum:number}
 | {type:'newSession';name:string;bpm:number;instrument?:'guitar'|'drums'}
 | {type:'cancelStartupScan'}
 | {type:'stereoPan';id:number;left:number;right:number}
 | {type:'stockPlugin';channel:number;slot:number;name:'NAM'|'Bus Compressor'}
 | {type:'trackMenu';id:number}
 | {type:'midiBatch';track:number;action:'add'|'update'|'delete';notes:MidiNote[];ids:number[]}
 | {type:'midiDrop';track:number;beat:number;data:string;request:string}
 | {type:'autosave';enabled?:boolean;now?:boolean}
 | {type:'recoverSession';id:string}
 | {type:'recoveryFolder'}
 | {type:'groupEdit';action:'split'|'delete'|'move'|'undo'|'redo';tracks?:number[];delta?:number}
 | {type:'automationGesture';track:number;parameter:'volume'|'pan';active:boolean}
 | {type:'automationBypass'}
 | {type:'recordingAlignment';enabled:boolean}
 | {type:'pluginAutomation';channel:number;slot:number;action:'select'|'put'|'delete'|'clear'|'mode'|'writeMode'|'undo'|'redo';parameter?:string;time?:number;value?:number;read?:boolean;mode?:number}
 | {type:'automation';track:number;action:'mode'|'put'|'delete'|'clear'|'undo'|'redo';parameter?:'volume'|'pan';mode?:number;time?:number;value?:number}
 | {type:'playlist';track:number;action:'new'|'duplicate'|'rename'|'switch'|'promote'|'up'|'down'|'delete'|'undo'|'redo';id?:number;name?:string}
 | { type:'routing';id:number;output?:number;send?:number;target?:number;pre?:boolean;hardwarePair?:number }
 | {type:'monitorArmed';enabled:boolean}
 | { type:'sendLevel';id:number;send:number;gain:number }
 | { type: 'exportMix'; selection:boolean; bits:16|24; tail:number }
 | { type: 'cancelExport' }
 | { type: 'transport'; playing: boolean; bpm: number; loop: boolean }
 | { type: 'seek'; beat: number }
 | { type: 'undo' | 'redo'; track?:number }
 | { type: 'midiEdit'; track:number; action:'add'|'update'|'delete'; id:number; pitch?:number; channel?:number; start?:number; length?:number; velocity?:number }
 | { type: 'midiImport'|'midiExport'|'grooveToMidi'; track:number }
 | { type: 'clipEdit'; track: number; id: number; action: 'move'|'trimLeft'|'trimRight'|'split'|'delete'|'trimSelection'|'fadeIn'|'fadeOut'|'crossfade'; position: number }
 | { type: 'editRange'; start: number; end: number }
 | { type: 'playStart'; position: number }
 | { type: 'dynamicTransport'|'punch'; enabled: boolean }
 | { type: 'newTrackDialog' } | { type: 'addTrack'; kind?:'AUDIO'|'MIDI'|'AUX' }
 | { type: 'stop' | 'ready' | 'devices' | 'saveSession' | 'openSession' | 'testOutput' }
 | { type:'generationLibrary' }
 | { type: 'rename'; id: number; name: string }
 | { type: 'metronome'; enabled: boolean }
 | { type: 'preRoll'; bars: number }
 | { type: 'clickLevel'; gain: number }
 | { type: 'record' | 'recordMode'; channels: 1 | 2; id?:number }
 | { type: 'monitor'; enabled: boolean; gain: number; id: number }
 | { type: 'inputSource'; index: number; id?:number }
 | { type: 'import'; id: number }
 | { type: 'clear'; id: number }
 | { type: 'analyseDrums'; id: number }
 | { type: 'grooveTempo' | 'grooveExport' }
 | { type: 'pluginLoad' | 'pluginEditor' | 'pluginBypass' | 'pluginRemove'; channel: number; slot: number }
 | { type: 'master'; gain: number }
 | { type: 'track'; id: number; gain: number; pan: number; muted: boolean; solo: boolean; armed: boolean };
export type AudioTrackState = { peakLeft?:number;peakRight?:number; id: number; fileName: string; duration: number; peak: number; waveform?: number[] };
export type ProjectTrack = { clickTrack?:boolean;drumNoteMap?:number[];ezInstrument?:'guitar'|'drums'; stereoLeft?:number;stereoRight?:number; output?:number;hardwarePair?:number;sends?:{target:number;gain:number;pre?:boolean}[]; inputSource?:number;inputChannels?:1|2; kind?:string; id: number; name: string; gain: number; pan: number; muted: boolean; solo: boolean; armed: boolean };
export type InsertState = { token?:number; channel: number; slot: number; name: string; bypass: boolean; missing: boolean; latency: number; compensation: number };
export type TimelineClip = { fadeIn?:number;fadeOut?:number;fadeOffset?:number;fadeSpan?:number;blend?:boolean; id:number; track:number; source:number; start:number; offset:number; length:number; sourceDuration:number; name:string; waveform?:number[] };
export type MidiNote = { track:number;id:number;pitch:number;channel:number;start:number;length:number;velocity:number };
export type PlaylistInfo={track:number;id:number;name:string;active:boolean;clips:TimelineClip[];notes:MidiNote[]};
export type MidiDropResult={request:string;track:number;success:boolean;message:string;ids:number[]};
export type AudioState = { io?:IOState; captureActive?:boolean;recordingStart?:number; generationLibrary?:string; bpm?:number; nativeGrooveAnalysis?:{track:number;source:number;busy:boolean;valid:boolean;bpm:number;confidence:number;hits:number;status:string}; sessionRevision?:number; startupScan?:{active:boolean;done:number;total:number;failed:number;name:string}; recordingWaveforms?:{track:number;waveform:number[]}[]; peakLeft?:number;peakRight?:number; midiDropResult?:MidiDropResult; autosaveEnabled?:boolean;autosaveBusy?:boolean;autosaveTime?:number;autosaveNotice?:string;recoveries?:{id:string;name:string;time:number}[]; canBatchUndo?:boolean;canBatchRedo?:boolean; monitorArmed?:boolean;monitorTracks?:number[];outputPairs?:{id:number;name:string}[]; automationBypass?:boolean;recordingAlignment?:boolean;recordingCorrection?:number; pluginAutomation?:PluginAutomationState; automation?:AutomationTrack[];canAutomationUndo?:boolean;canAutomationRedo?:boolean; playlists?:PlaylistInfo[]; recordingTracks?:number[]; exportBusy?:boolean; exportProgress?:number; midiNotes?:MidiNote[];canMidiUndo?:boolean;canMidiRedo?:boolean; clips?:TimelineClip[]; canUndo?:boolean; canRedo?:boolean; dynamicTransport?:boolean; punchEnabled?:boolean; playStart?:number; selectionStart?:number; selectionEnd?:number; countInBeats?: number; preRollBars?: number; metronomeGain?: number; cpuLoad?: number; xruns?: number; debugBuild?: boolean; deviceOutputPeak?: number; callbackOutputs?: number; writableOutputs?: number; outputNames?: string; driverError?: string; driver?: string; deviceName?: string; bufferSize?: number; inputLatency?: number; outputLatency?: number; monitorActive?: boolean; monitorEnabled?: boolean; monitorTrack?: number; monitorPeak?: number; monitorGain?: number; inputSource?: number; inputNames?: string[]; inputPeaks?: number[]; playbackLatency?: number; groove?: {track: number; bpm: number; confidence: number; hits: number; seconds: number; pattern: number[][]}; inserts?: InsertState[]; pluginBusy?: boolean; playing: boolean; seconds: number; duration: number; peak: number; clipping: boolean; sampleRate: number; tracks: AudioTrackState[]; loading: boolean; status: string; sessionName: string; dirty: boolean; recording: boolean; recordingBusy: boolean; recordingTrack: number; recordSeconds: number; inputPeak: number; inputChannels: number; project?: { bpm: number; master: number; loop: boolean; metronome: boolean; tracks: ProjectTrack[]; selectTrack: number } };
declare global {
 interface Window { __JUCE__?: { backend?: {
  emitEvent: (event: string, payload: Command) => void;
  addEventListener: (event: string, callback: (state: AudioState) => void) => [string, number];
  removeEventListener: (handle: [string, number]) => void;
 } } }
}
// Control and throttled meter messages only. No audio buffers cross the bridge.
export const isNative = () => !!window.__JUCE__?.backend;
export function send(command: Command) { window.__JUCE__?.backend?.emitEvent('command', command); }
export function subscribe(callback: (state: AudioState) => void) {
 const backend = window.__JUCE__?.backend;
 if (!backend) return () => {};
 const handle = backend.addEventListener('audioState', callback);
 send({type:'ready'});
 return () => backend.removeEventListener(handle);
}
