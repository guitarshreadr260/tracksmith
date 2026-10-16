import {readFileSync} from 'node:fs';
import ts from 'typescript';
import assert from 'node:assert/strict';
const helperSource=readFileSync(new URL('../src/midiDrop.ts',import.meta.url),'utf8');const helper=ts.transpileModule(helperSource,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.CommonJS}}).outputText;const helperExports={};new Function('exports',helper)(helperExports);
const source=readFileSync(new URL('../src/InstrumentPianoRoll.tsx',import.meta.url),'utf8');
const start=source.indexOf('const drop=async'),end=source.indexOf(' const finish=',start);
assert.ok(start>=0&&end>start);
const handler=ts.transpileModule(source.slice(start,end)+';return drop;',{compilerOptions:{target:ts.ScriptTarget.ES2022}}).outputText;
async function drop(files,disabled=false){const commands=[],messages=[],pendingDrop={current:''};let selected=0,prevented=0;const fn=new Function('midiDropFiles','midiDropData','disabled','onSelect','point','snap','grid','track','setMessage','pendingDrop','send',handler)(helperExports.midiDropFiles,helperExports.midiDropData,disabled,()=>++selected,()=>({x:3.39,y:800}),true,.25,2,m=>messages.push(m),pendingDrop,c=>commands.push(c));await fn({preventDefault(){++prevented;},stopPropagation(){},dataTransfer:{files}});return{commands,messages,pendingDrop,selected,prevented};}
const bytes=Buffer.from('4d546864000000060000000101e04d54726b0000000c009924640a89240000ff2f00','hex');
const file={name:'GGD Groove.MID',size:bytes.length,arrayBuffer:async()=>bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength)};
const result=await drop([file]);assert.equal(result.commands.length,1);assert.equal(result.commands[0].track,2);assert.equal(result.commands[0].beat,3.25);assert.equal(result.commands[0].data,bytes.toString('base64'));assert.equal(result.commands[0].request,result.pendingDrop.current);assert.ok(result.pendingDrop.current.length>0);assert.equal(result.messages.at(-1),'Importing MIDI groove…');
assert.equal((await drop([file],true)).commands.length,0);
for(const files of [[],[file,file],[{...file,name:'audio.wav'}],[{...file,size:1048577}]]){const result=await drop(files);assert.equal(result.commands.length,0);assert.match(result.messages[0],/Drop one|readable MIDI/);}
const failed=await drop([{...file,arrayBuffer:async()=>{throw new Error('unreadable temp MIDI');}}]);assert.equal(failed.commands.length,0);assert.match(failed.messages.at(-1),/Could not read/);
console.log('MIDI drop byte transfer, grid position, result request, size/type guards and read failure: PASS');

assert.deepEqual(helperExports.midiDropFiles({files:[],items:[{kind:'file',getAsFile:()=>file}]}),[file]);
console.log('Plugin DataTransfer.items file fallback PASS');
