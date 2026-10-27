(function(root,factory){const api=factory();if(typeof module==='object'&&module.exports)module.exports=api;else root.RiffCore=api;})(globalThis,function(){
 'use strict';
 const map={36:24,38:26,42:43,46:46,45:35,47:34,50:33,49:52,51:62};
 function plan(data,options={}){
  const swing=Number(options.swing??50);if(!Number.isFinite(swing)||swing<50||swing>75)throw Error('Swing must be 50-75.');
  const hits=[],used=new Set(),start=(data.start_bar-1)*1920,end=data.end_tick;
  const onsets=data.onsets.filter(o=>o.enabled&&Number.isFinite(o.tick)&&o.tick>=start&&o.tick<end);
  function quiet(tick){if(onsets.some(o=>Math.abs(o.tick-tick)<=120))return false;return data.rests.some(r=>r.start_tick<=tick&&tick<r.end_tick);}
  function add(tick,note,velocity){
   if(note===42||note===46||note===51){if(tick%480===240)tick+=Math.round(480*swing/100)-240;}
   if(tick<start||tick>=end||(options.respectRests!==false&&quiet(tick)))return;
   const key=tick+':'+note;if(used.has(key))return;used.add(key);hits.push({tick,note:map[note],velocity});
  }
  // Original, editable four-bar rock vocabulary. Steps are sixteenths.
  const grooves=[
   {name:'Driving rock',kick:[[0,8],[0,6,8],[0,3,8,10],[0,6,8,12]],snare:[[4,12],[4,12],[4,12],[4,11,12]]},
   {name:'Syncopated rock',kick:[[0,6,10],[0,7,10],[0,3,8,11],[0,6,10,14]],snare:[[4,12],[4,12],[4,10,12],[4,12]]},
   {name:'Half-time heavy',kick:[[0,6,10],[0,8],[0,3,8,10],[0,6,10,14]],snare:[[8],[8],[8],[8,14]]}
  ];
  const variation=Math.max(0,Math.min(2,Number(options.variation??1))); const follow=Math.max(0,Math.min(2,Number(options.follow??1))); const energy=Math.max(0,Math.min(2,Number(options.energy??1))); let phraseIndex=0, chosen=null;
  for(const bar of data.bars){
   const base=(bar.number-1)*1920;if(base<start||base>=end)continue;
   if(bar.phrase_start){phraseIndex=0;chosen=null;}
   const phase=phraseIndex++%4;if(bar.mute)continue;
   const active=onsets.filter(o=>o.tick>=base&&o.tick<base+1920);
   const guitar=active.map(o=>({step:Math.round((o.tick-base)/120),strength:o.strength})).filter(o=>o.step>=0&&o.step<16);
   if(!chosen){
    // Pick a vocabulary by kick alignment over up to four bars, with a mild preference for driving rock.
    const window=onsets.filter(o=>o.tick>=base&&o.tick<Math.min(end,base+7680));
    chosen=grooves.map((g,i)=>({g,score:window.reduce((sum,o)=>{const step=Math.round(((o.tick-base)%1920)/120)%16;return sum+(g.kick[0].includes(step)?o.strength:0)},i===0?0.15:0)})).sort((a,b)=>b.score-a.score)[0].g;
   }
   const pattern=variation===0?0:phase;
   let kick=chosen.kick[pattern].slice(), snare=chosen.snare[pattern].slice();
   if(follow>0){
    const candidate=guitar.filter(o=>o.strength>=0.55&&!snare.includes(o.step)&&!kick.some(k=>Math.abs(k-o.step)<2)).sort((a,b)=>b.strength-a.strength||a.step-b.step)[0];
    if(candidate&&kick.length<(follow===2?6:4))kick.push(candidate.step);
    if(follow===2){const extra=guitar.filter(o=>o.strength>=.65&&!snare.includes(o.step)&&!kick.some(k=>Math.abs(k-o.step)<2)).sort((a,b)=>b.strength-a.strength)[0];if(extra&&kick.length<6)kick.push(extra.step);}
    if(variation===2&&phase===1&&!snare.includes(11))snare.push(11);
   }
   const next=data.bars.find(b=>b.number===bar.number+1);
   const fill=!!options.fills&&!!next&&!next.mute&&(next.phrase_start||phase===3)&&base+1920<=end;
   if(fill){kick=kick.filter(t=>t<12);snare=snare.filter(t=>t<12);}
   for(const step of kick){const strength=guitar.filter(o=>Math.abs(o.step-step)<=1).reduce((v,o)=>Math.max(v,o.strength),0);add(base+step*120,36,Math.round(92+Math.min(1,strength)*16));}
   for(const step of snare)add(base+step*120,38,Math.max(1,Math.min(127,(step===4||step===8||step===12?108:step===11?55:85)+(energy-1)*12)));
   for(let step=0;step<16;step+=2){if(fill&&step>=12)continue;if(variation>0&&phase===3&&step===14)continue;add(base+step*120,options.ride?51:42,Math.max(1,Math.min(127,(step%4===0?78:61)+(energy-1)*10)));}
   if(energy===2&&!fill&&phase===2)add(base+1680,46,73);
   if(options.crashes&&bar.phrase_start)add(base,49,105);
   if(fill)for(const [tick,note,vel] of [[1440,38,86],[1560,50,90],[1680,47,95],[1800,45,100]])add(base+tick,note,vel);
  }
  return hits.sort((a,b)=>a.tick-b.tick||a.note-b.note);
 }
 function vlq(n){if(!Number.isInteger(n)||n<0||n>0xfffffff)throw Error('Invalid MIDI delta');const b=[n&127];while((n>>>=7)>0)b.unshift((n&127)|128);return b;}
 function midi(data,options={},editedHits=null){
  if(!Number.isFinite(data.bpm)||data.bpm<30||data.bpm>300)throw Error('Invalid tempo');
  const hits=editedHits||plan(data,options),events=[];for(const h of hits){events.push([h.tick,[0x99,h.note,h.velocity]]);events.push([Math.min(h.tick+60,data.end_tick),[0x89,h.note,0]]);}
  events.sort((a,b)=>a[0]-b[0]||a[1][0]-b[1][0]);const tempo=Math.round(60000000/data.bpm);
  const track=[0,255,81,3,(tempo>>>16)&255,(tempo>>>8)&255,tempo&255,0,255,88,4,4,2,24,8];let last=0;
  for(const [tick,message] of events){track.push(...vlq(tick-last),...message);last=tick;}
  track.push(...vlq(Math.max(last,data.end_tick)-last),255,47,0);
  const n=track.length;return new Uint8Array([77,84,104,100,0,0,0,6,0,0,0,1,1,224,77,84,114,107,(n>>>24)&255,(n>>>16)&255,(n>>>8)&255,n&255,...track]);
 }
 return {plan,midi};
});
