import type {AudioState,ProjectTrack} from './bridge';
export type IOOption={id:number;name:string;available:boolean};
export function inputPaths(audio:AudioState,width:number,current=-1):IOOption[]{
 const channels=audio.io?.inputs??(audio.inputNames??[]).map((name,id)=>({id,name,hardwareName:name,active:true}));
 const paths:IOOption[]=[{id:-1,name:'No input',available:true}];
 for(const ch of channels){const right=channels.find(p=>p.id===ch.id+1);if(width===2&&!right)continue;
  const available=ch.active&&(width!==2||!!right?.active);
  paths.push({id:ch.id,name:`${ch.id+1}${width===2?`–${ch.id+2}`:''} · ${ch.name}${width===2?` / ${right!.name}`:''}${available?'':' (disabled)'}`,available});
 }
 if(!paths.some(p=>p.id===current))paths.push({id:current,name:`Input ${current+1}${width===2?`–${current+2}`:''} (unavailable)`,available:false});
 return paths;
}
export function outputPaths(audio:AudioState):IOOption[]{
 const channels=audio.io?.outputs??[];
 const result:IOOption[]=[];
 for(let id=0;id<8;id++){const left=channels.find(c=>c.id===id*2),right=channels.find(c=>c.id===id*2+1);
  if(!left&&!right&&id!==audio.io?.mainOutputPair)continue;
  const mono=id===0&&channels.length===1&&!!left;
  result.push({id,name:left?`${id*2+1}${mono?'':`–${id*2+2}`} · ${left.name}${right?` / ${right.name}`:''}`:`Output ${id*2+1}–${id*2+2} (unavailable)`,available:!!left?.active&&(mono||!!right?.active)});
 }
 return result;
}
export function inputAvailable(audio:AudioState,track:ProjectTrack):boolean{
 return audio.io?.matched!==false&&!!inputPaths(audio,track.inputChannels??1,track.inputSource??0).find(p=>p.id===(track.inputSource??0)&&p.id>=0)?.available;
}
