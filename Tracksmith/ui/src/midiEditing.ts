import type {MidiNote} from './bridge';
export const noteName=(pitch:number)=>['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B'][pitch%12]+(Math.floor(pitch/12)-1);
export const snapBeat=(beat:number,grid:number)=>Math.round(beat/grid)*grid;
export function dragNote(note:MidiNote,action:'move'|'resize',deltaBeats:number,deltaPitch:number,grid:number,snap:boolean):MidiNote{
 const step=snap?grid:.01;
 if(action==='resize')return {...note,length:Math.max(step,Math.min(100000-note.start,snap?snapBeat(note.length+deltaBeats,grid):note.length+deltaBeats))};
 const start=Math.max(0,Math.min(100000-note.length,snap?snapBeat(note.start+deltaBeats,grid):note.start+deltaBeats));
 return {...note,start,pitch:Math.max(0,Math.min(127,note.pitch+Math.round(deltaPitch)))};
}

export function marqueeNotes(notes:MidiNote[],a:number,b:number,low:number,high:number):number[]{
 return notes.filter(n=>n.start<=Math.max(a,b)&&n.start+n.length>=Math.min(a,b)&&n.pitch>=low&&n.pitch<=high).map(n=>n.id);
}
export function transformNotes(notes:MidiNote[],delta:number,pitch:number,resize=false):MidiNote[]{
 if(!notes.length)return [];
 const dt=resize?Math.max(.01-Math.min(...notes.map(n=>n.length)),Math.min(delta,100000-Math.max(...notes.map(n=>n.start+n.length)))):Math.max(-Math.min(...notes.map(n=>n.start)),Math.min(delta,100000-Math.max(...notes.map(n=>n.start+n.length))));
 const dp=Math.max(-Math.min(...notes.map(n=>n.pitch)),Math.min(pitch,127-Math.max(...notes.map(n=>n.pitch))));
 return notes.map(n=>({...n,start:n.start+(resize?0:dt),length:n.length+(resize?dt:0),pitch:n.pitch+(resize?0:dp)}));
}

export function noteViewTop(notes:MidiNote[],rowHeight=18):number{
 const pitches=notes.map(n=>n.pitch).sort((a,b)=>a-b),center=pitches.length?pitches[Math.floor(pitches.length/2)]:60;
 return Math.max(0,(127-Math.min(127,center+6))*rowHeight);
}
