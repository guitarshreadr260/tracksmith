export const gridChoices=[
 {label:'1 bar',beats:4},{label:'1/4',beats:1},{label:'1/8',beats:.5},
 {label:'1/16',beats:.25},{label:'1/32',beats:.125},{label:'1/8 triplet',beats:1/3},
];
export function snapBeat(beat:number,step:number){return Math.max(0,Math.round(beat/step)*step)}
export function musicalGrid(total:number,width:number,step:number){
 const px=Math.max(1,Number.isFinite(width)?width:1000)/Math.max(1,total);
 let labelBars=1;while(px*4*labelBars<72)labelBars*=2;
 let majorBars=1;while(px*4*majorBars<10)majorBars*=2;
 const labels=[] as {beat:number;bar:number;beatInBar:number;major:boolean}[];
 const every=px>=48?1:labelBars*4;
 for(let beat=0;beat<total-1.e-8;beat+=every)labels.push({beat,bar:Math.floor(beat/4)+1,beatInBar:beat%4+1,major:beat%4===0});
 return {labels,barPixels:px*4*majorBars,beatPixels:px>=8?px:0,subPixels:step<1&&px*step>=8?px*step:0};
}
