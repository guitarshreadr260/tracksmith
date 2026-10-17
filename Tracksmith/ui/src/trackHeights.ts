export const maximumTrackHeight=900;
export function trackHeight(value:number|undefined,midi=false){const fallback=midi?323:126;return Math.round(Math.max(midi?216:126,Math.min(maximumTrackHeight,typeof value==='number'&&Number.isFinite(value)&&value>0?value:fallback)))}
export function visibleNoteRows(height:number){return Math.max(12,Math.min(88,Math.floor((height-107)/9)))}
