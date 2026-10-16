import React,{useState} from 'react';
import type {AudioState,Command,ProjectTrack} from './bridge';
export type GlyphKind='audio'|'guitar'|'bass'|'drums'|'vocal'|'keys'|'synth'|'folder'|'midi'|'fx'|'routing';
const paths:Record<GlyphKind,string>={
 audio:'M2 12h3m2-5v10m3-14v18m4-15v12m3-10v8m3-5h2',
 guitar:'M15 3l6 1-1 6-3-3-6 6c1 3 0 6-3 7s-6-1-6-4 3-5 6-4l6-6-2-2zM6 15l3 3',
 bass:'M15 3l6 1-1 6-3-3-6 6c1 3 0 6-3 7s-6-1-6-4 3-5 6-4l6-6-2-2zM5 16h4',
 drums:'M4 9c0-3 16-3 16 0v9c0 3-16 3-16 0zm0 0c0 3 16 3 16 0M8 11v8m8-8v8M3 2l12 4M21 2L9 6',
 vocal:'M9 5a3 3 0 016 0v7a3 3 0 01-6 0zm-3 5v2a6 6 0 0012 0v-2m-6 8v4m-4 0h8',
 keys:'M3 5h18v15H3zm6 0v15m6-15v15M6 5v8h3m3-8v8h3m3-8v8h3',
 synth:'M2 13h3l3-8 4 15 4-13 3 6h3',
 folder:'M3 6h6l2 3h10v11H3zm0 3h18',
 midi:'M5 7a9 9 0 1114 12H5A9 9 0 015 7zm2 3h.1m5-3h.1m5 3h.1m-10 5h.1m10 0h.1M9 20v-4h6v4',
 fx:'M12 2v5m0 10v5M2 12h5m10 0h5M5 5l3 3m8 8 3 3M5 19l3-3m8-8 3-3M9 12l3-3 3 3-3 3z',
 routing:'M4 4h5v5H4zm11 11h5v5h-5M9 6h8v9M6 9v8h9',
};
export function StudioGlyph({kind}:{kind:GlyphKind}){return <svg className="studio-glyph" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true"><path d={paths[kind]}/></svg>}
export function instrumentKind(t:ProjectTrack):GlyphKind{
 const name=t.name.toLowerCase();if(t.ezInstrument==='drums'||/drum|kick|snare|hat|tom|cymbal/.test(name))return 'drums';
 if(/bass/.test(name))return 'bass';if(t.ezInstrument==='guitar'||/guitar|gtr/.test(name))return 'guitar';
 if(/vocal|voice|vox/.test(name))return 'vocal';if(/piano|keys/.test(name))return 'keys';if(/synth/.test(name))return 'synth';
 return t.kind==='AUX'||t.clickTrack?'fx':t.kind==='MIDI'?'midi':'audio';
}
export function trackColor(t:ProjectTrack,index:number){
 const palette:Partial<Record<GlyphKind,string>>={drums:'#39b6df',bass:'#df955c',guitar:'#43cbb6',vocal:'#a27bdb',keys:'#d0ac57',synth:'#638ee1',fx:'#98afbc'};
 return palette[instrumentKind(t)]??['#45c6c1','#6d9dd9','#b18bd3','#d9a262'][index%4];
}
export function Wordmark(){return <span className="studio-wordmark">TR<span>A</span>CKSMITH</span>}
export function StudioLibrary({audio,chosen,disabled,command,onInstruments,onMix,onIO}:{audio:AudioState;chosen?:ProjectTrack;disabled:boolean;command:(c:Command)=>void;onInstruments:()=>void;onMix:()=>void;onIO:()=>void}){
 const [query,setQuery]=useState('');
 const items:{name:string;kind:GlyphKind;group:string;hint:string;disabled?:boolean;action:()=>void}[]=[
  {name:'Open session',kind:'folder',group:'PROJECT',hint:'Choose a Tracksmith session',action:()=>command({type:'openSession'})},
  {name:'Import audio',kind:'audio',group:'ADD TO SESSION',hint:'Import a WAV onto the selected audio track',disabled:chosen?.kind!=='AUDIO',action:()=>chosen&&command({type:'import',id:chosen.id})},
  {name:'Import MIDI',kind:'midi',group:'ADD TO SESSION',hint:'Import MIDI onto the selected instrument track',disabled:chosen?.kind!=='MIDI',action:()=>chosen&&command({type:'midiImport',track:chosen.id})},
  {name:'Native instruments',kind:'keys',group:'SOUNDS & TOOLS',hint:'Add guitar with NAM or drums with Groove built in',action:onInstruments},
  {name:'Groove library',kind:'drums',group:'SOUNDS & TOOLS',hint:'Choose your reference groove folder',action:()=>command({type:'generationLibrary'})},
  {name:'Add effect',kind:'fx',group:'SOUNDS & TOOLS',hint:'Open the stock/VST3 picker for the next empty insert',disabled:!chosen||(audio.inserts??[]).filter(i=>i.channel===chosen.id).length>=5,action:()=>{if(chosen){const slot=[0,1,2,3,4].find(s=>!audio.inserts?.some(i=>i.channel===chosen.id&&i.slot===s));if(slot!==undefined)command({type:'pluginLoad',channel:chosen.id,slot})}}},
  {name:'Mix console',kind:'routing',group:'STUDIO',hint:'Open the separate mixer',action:onMix},
  {name:'I/O setup',kind:'routing',group:'STUDIO',hint:'Hardware inputs, outputs, and bus assignments',action:onIO},
 ];
 const visible=items.filter(i=>i.name.toLowerCase().includes(query.toLowerCase()));
 return <aside className="studio-library" aria-label="Studio library"><div className="library-heading"><span>LIBRARY</span><small>SESSION TOOLS</small></div><label className="library-search"><span aria-hidden="true">⌕</span><input aria-label="Search library actions" placeholder="Find a tool…" value={query} onChange={e=>setQuery(e.target.value)}/></label><nav>{visible.map((item,i)=><React.Fragment key={item.name}>{(i===0||visible[i-1].group!==item.group)&&<small className="library-group">{item.group}</small>}<button title={item.hint} disabled={disabled||item.disabled} onClick={item.action}><StudioGlyph kind={item.kind}/><span>{item.name}</span></button></React.Fragment>)}{!visible.length&&<p className="library-no-results">No matching tools.</p>}</nav><div className="library-brand-art" role="img" aria-label="Tracksmith studio — Shape your sound"/><div className="library-device"><span className={audio.sampleRate?'device-ready':''}/><div><strong>{audio.deviceName||'Audio device'}</strong><small>{audio.sampleRate?`${audio.sampleRate/1000} kHz · ${audio.bufferSize||0} samples`:'Choose a device in I/O setup'}</small></div></div></aside>
}
