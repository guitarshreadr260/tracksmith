export type CounterFormat='bars'|'time'|'samples';
export function formatCounter(seconds:number,bpm:number,rate:number,format:CounterFormat,length=false){
 const value=Math.max(0,Number.isFinite(seconds)?seconds:0);
 if(format==='samples')return String(Math.round(value*Math.max(1,rate)));
 if(format==='bars'){const ticks=Math.round(value*bpm/60*960),bars=Math.floor(ticks/3840),within=ticks%3840;return `${bars+(length?0:1)} | ${Math.floor(within/960)+(length?0:1)} | ${String(within%960).padStart(3,'0')}`}
 const ms=Math.round(value*1000),hours=Math.floor(ms/3600000),minutes=Math.floor(ms/60000)%60,s=Math.floor(ms/1000)%60;return `${hours}:${String(minutes).padStart(2,'0')}:${String(s).padStart(2,'0')}.${String(ms%1000).padStart(3,'0')}`;
}
export function parseCounter(text:string,bpm:number,rate:number,format:CounterFormat,length=false):number|null{
 const raw=text.trim();if(!raw)return null;let seconds=NaN;
 if(format==='samples'){if(!/^\d+$/.test(raw))return null;seconds=Number(raw)/Math.max(1,rate)}
 else if(format==='bars'){const m=/^(\d+)\s*[|.]\s*(\d+)\s*[|.]\s*(\d+)$/.exec(raw);if(!m)return null;const bar=+m[1]-(length?0:1),beat=+m[2]-(length?0:1),tick=+m[3];if(bar<0||beat<0||beat>3||tick>959)return null;seconds=(bar*4+beat+tick/960)*60/bpm}
 else{if(!/^(?:\d+:){0,2}\d+(?:\.\d{1,6})?$/.test(raw))return null;const parts=raw.split(':').map(Number);if(parts.slice(1).some(n=>n>=60))return null;seconds=parts.reduce((v,n)=>v*60+n,0)}
 return Number.isFinite(seconds)&&seconds>=0&&seconds<=86400?seconds:null;
}
export function selectionRoll(start:number,end:number,bpm:number,preBars:number,postBars:number,mediaEnd:number){
 if(![start,end,bpm,preBars,postBars,mediaEnd].every(Number.isFinite)||start<0||end<=start||bpm<20||bpm>300||preBars<0||preBars>8||postBars<0||postBars>8||mediaEnd<=start)return null;
 return {start:Math.max(0,start-preBars*4*60/bpm),end:Math.min(mediaEnd,end+postBars*4*60/bpm)};
}
