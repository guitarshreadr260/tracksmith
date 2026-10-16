import type {ElasticAnchor} from './bridge';
export function validElasticMap(anchors:ElasticAnchor[],length:number){
 if(!Number.isFinite(length)||length<.5||length>30||anchors.length<2||anchors.length>16)return false;
 if(Math.abs(anchors[0].source)>1e-7||Math.abs(anchors[0].target)>1e-7||Math.abs(anchors.at(-1)!.source-length)>1e-7||Math.abs(anchors.at(-1)!.target-length)>1e-7)return false;
 return anchors.every((p,i)=>{if(!Number.isFinite(p.source)||!Number.isFinite(p.target))return false;if(!i)return true;const input=p.source-anchors[i-1].source,output=p.target-anchors[i-1].target;return input>=.05&&output>=.05&&output/input>=.75-1e-7&&output/input<=1.5+1e-7});
}
export function addElasticAnchor(anchors:ElasticAnchor[]){
 if(anchors.length>=16)return anchors;let segment=1;for(let i=2;i<anchors.length;i++)if(anchors[i].source-anchors[i-1].source>anchors[segment].source-anchors[segment-1].source)segment=i;
 const a=anchors[segment-1],b=anchors[segment];return [...anchors.slice(0,segment),{source:(a.source+b.source)/2,target:(a.target+b.target)/2},...anchors.slice(segment)];
}
export function snapWarpTime(time:number,origin:number,bpm:number,grid:number){const step=grid*60/bpm;return Math.round((origin+time)/step)*step-origin}
export function quantizeElasticMap(base:ElasticAnchor[],length:number,origin:number,bpm:number,grid:number,strength:number){
 if(!validElasticMap(base,length)||!Number.isFinite(origin)||!Number.isFinite(bpm)||bpm<20||bpm>300||!Number.isFinite(grid)||grid<=0||!Number.isFinite(strength))return {anchors:base,appliedStrength:0};
 const requested=Math.max(0,Math.min(1,strength));
 const candidate=(amount:number)=>base.map((p,i)=>i===0||i===base.length-1?{...p}:{...p,target:p.target+(snapWarpTime(p.target,origin,bpm,grid)-p.target)*amount});
 let amount=requested;if(!validElasticMap(candidate(amount),length)){let low=0,high=requested;for(let i=0;i<32;i++){const middle=(low+high)/2;if(validElasticMap(candidate(middle),length))low=middle;else high=middle;}amount=low;}
 return {anchors:candidate(amount),appliedStrength:amount};
}
