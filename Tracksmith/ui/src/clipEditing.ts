import type {TimelineClip} from './bridge';
export type ClipGesture='move'|'trimLeft'|'trimRight'|'fadeIn'|'fadeOut';
export function previewClip(clip:TimelineClip,action:ClipGesture,delta:number,snap:boolean,bpm:number,grid:number):TimelineClip{
 const quantize=(s:number)=>snap?Math.round(s*bpm/60/grid)*grid*60/bpm:s;
 const next={...clip};
 if(action==='move')next.start=Math.max(0,Math.min(86400-clip.length,quantize(clip.start+delta)));
 if(action==='trimLeft'){const start=Math.max(Math.max(0,clip.start-clip.offset),Math.min(clip.start+clip.length-.001,quantize(clip.start+delta)));next.start=start;next.offset=clip.offset+start-clip.start;next.length=clip.length-(start-clip.start)}
 if(action==='trimRight')next.length=Math.max(.001,Math.min(clip.sourceDuration-clip.offset,quantize(clip.start+clip.length+delta)-clip.start));
 if(action==='fadeIn'){next.fadeIn=Math.max(0,Math.min(clip.length,(clip.fadeIn??0)+delta));next.fadeOut=Math.min(clip.fadeOut??0,clip.length-next.fadeIn)}
 if(action==='fadeOut'){next.fadeOut=Math.max(0,Math.min(clip.length,(clip.fadeOut??0)-delta));next.fadeIn=Math.min(clip.fadeIn??0,clip.length-next.fadeOut)}
 return next;
}
export function editPosition(clip:TimelineClip,action:ClipGesture){return action==='trimRight'?clip.start+clip.length:action==='fadeIn'?clip.fadeIn??0:action==='fadeOut'?clip.fadeOut??0:clip.start}
