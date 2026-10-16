const fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const {MusicVAE}=require('@magenta/music/node/music_vae');
const tf=require('@tensorflow/tfjs');
async function main(){
 const [requestFile,resultFile]=process.argv.slice(2);if(!requestFile||!resultFile)throw Error('Request and result paths required');
 if(fs.statSync(requestFile).size>262144)throw Error('Request too large');const request=JSON.parse(fs.readFileSync(requestFile,'utf8'));
 if(!Number.isFinite(request.tempo)||request.tempo<20||request.tempo>300||!Array.isArray(request.windows)||request.windows.length<1||request.windows.length>8)throw Error('Invalid neural request');
 const root=path.join(__dirname,'GrooVAE');const allowed=new Set(['config.json','weights_manifest.json','group1-shard1of4','group1-shard2of4','group1-shard3of4','group1-shard4of4']);
 const server=http.createServer((q,r)=>{const name=q.url.slice(1);if(!allowed.has(name)){r.statusCode=404;r.end();return;}r.setHeader('Content-Type',name.endsWith('.json')?'application/json':'application/octet-stream');fs.createReadStream(path.join(root,name)).pipe(r);});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const began=Date.now();const model=new MusicVAE('http://127.0.0.1:'+server.address().port);let notes=[];
 try{await tf.setBackend('cpu');await tf.ready();await model.initialize();
  for(let index=0;index<request.windows.length;index++){
   const window=request.windows[index];if(!Array.isArray(window.taps)||!window.taps.length||window.taps.length>32)continue;
   const sequence={quantizationInfo:{stepsPerQuarter:4},totalQuantizedSteps:32,tempos:[{time:0,qpm:request.tempo}],notes:window.taps.map(step=>{if(!Number.isInteger(step)||step<0||step>31)throw Error('Invalid rhythm step');return {pitch:42,velocity:100,isDrum:true,quantizedStartStep:step,quantizedEndStep:step+1};})};
   const refs=Array.isArray(window.referenceTaps)&&window.referenceTaps.length?window.referenceTaps:window.taps;
   const refSequence={...sequence,notes:refs.map(step=>({pitch:42,velocity:100,isDrum:true,quantizedStartStep:step,quantizedEndStep:step+1}))};
   const z=await model.encode([sequence,refSequence]);const influence=Math.max(0,Math.min(1,Number(request.influence)||0));const mixed=tf.tidy(()=>{const parts=tf.split(z,2,0);return parts[0].mul(1-influence).add(parts[1].mul(influence));});let decoded;try{decoded=await model.decode(mixed,0,undefined,4,request.tempo);}finally{z.dispose();mixed.dispose();}
   for(const n of decoded[0].notes||[]){const start=Math.max(0,index*8+(Number.isFinite(n.startTime)?n.startTime*request.tempo/60:n.quantizedStartStep/4));const end=index*8+(Number.isFinite(n.endTime)?n.endTime*request.tempo/60:n.quantizedEndStep/4);if(!Number.isFinite(start)||!Number.isFinite(end)||start<0||start>=request.beats)continue;notes.push({pitch:n.pitch,start,length:Math.min(.2,Math.max(.015,end-start)),velocity:Math.max(.05,Math.min(1,(n.velocity||80)/127))});}
  }
  if(!notes.length)throw Error('Neural model returned no drum notes');if(notes.length>2000)throw Error('Neural note limit exceeded');
  fs.writeFileSync(resultFile,JSON.stringify({ok:true,model:'GrooVAE tap2drum 2-bar',backend:'TensorFlow.js CPU',elapsedMs:Date.now()-began,notes}));
 }finally{model.dispose();await new Promise(r=>server.close(r));}
}
main().catch(e=>{try{fs.writeFileSync(process.argv[3],JSON.stringify({ok:false,error:String(e.message||e)}))}catch{}console.error(e);process.exitCode=1;});
