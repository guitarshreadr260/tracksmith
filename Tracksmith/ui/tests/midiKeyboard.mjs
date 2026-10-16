import {readFileSync} from 'node:fs';
import ts from 'typescript';
import assert from 'node:assert/strict';
import {transformNotes} from '../src/midiEditing.ts';
const source=readFileSync(new URL('../src/InstrumentPianoRoll.tsx',import.meta.url),'utf8');
const start=source.indexOf('const key=(e:KeyboardEvent)=>{'),end=source.indexOf(';window.addEventListener',start);
const handler=ts.transpileModule(source.slice(start,end)+';return key;',{compilerOptions:{target:ts.ScriptTarget.ES2022}}).outputText;
const notes=[{track:0,id:1,pitch:60,channel:1,start:0,length:1,velocity:.5},{track:0,id:2,pitch:64,channel:1,start:1,length:1,velocity:.5}];
function press(code,{ctrl=false,alt=false,shift=false,editable=false,menu=false,dialog=false,active=true,disabled=false,repeat=false}={}){
 const actions=[];let prevented=false;
 const funcs={active,disabled,notes,picked:notes,selected:[1],grid:.25,nextId:{current:2},transformNotes,setSelected:ids=>actions.push(['select',ids]),remove:()=>actions.push(['delete']),copy:()=>actions.push(['copy']),insert:dup=>actions.push([dup?'duplicate':'paste']),setTool:tool=>actions.push(['tool',tool]),batch:(action,items)=>actions.push([action,items]),setMenu:()=>{},setDrag:()=>{},setBox:()=>{}};
 const key=new Function(...Object.keys(funcs),handler)(...Object.values(funcs));
 key({code,ctrlKey:ctrl,altKey:alt,shiftKey:shift,metaKey:false,repeat,target:{closest:()=>editable||menu||dialog},preventDefault(){prevented=true;},stopImmediatePropagation(){}});
 return {actions,prevented};
}
for(const code of ['Delete','Backspace'])assert.deepEqual(press(code).actions,[['delete']]);
assert.deepEqual(press('KeyB',{ctrl:true}).actions,[['delete']]);
assert.deepEqual(press('KeyA',{ctrl:true}).actions,[['select',[1,2]]]);
assert.deepEqual(press('KeyX',{ctrl:true}).actions,[['copy'],['delete']]);
assert.deepEqual(press('KeyV',{ctrl:true}).actions,[['paste']]);
assert.deepEqual(press('KeyD',{ctrl:true}).actions,[['duplicate']]);
assert.deepEqual(press('F8').actions,[['tool','select']]);
assert.deepEqual(press('F10').actions,[['tool','draw']]);
assert.deepEqual(press('ArrowRight',{shift:true}).actions,[['select',[1,2]]]);
assert.deepEqual(press('ArrowDown',{shift:true}).actions[0][1].map(n=>n.pitch),[48,52]);
assert.deepEqual(press('ArrowUp',{alt:true}).actions[0][1].map(n=>[n.id,n.pitch]),[[3,61],[4,65]]);
assert.equal(press('ArrowUp',{ctrl:true}).actions[0][1][0].velocity,.5+5/127);
assert.equal(press('NumpadAdd').actions[0][1][0].start,.25);
for(const opts of [{editable:true},{active:false},{disabled:true},{repeat:true}])assert.deepEqual(press('Delete',opts).actions,[]);
assert.equal(press('KeyZ',{ctrl:true}).prevented,false); // Global Undo remains reachable.
console.log('MIDI shortcuts: selection/delete, tools, transpose/copy, velocity, clipboard, nudge and focus guards PASS');

for(const option of ['menu','dialog'])for(const code of ['Delete','ArrowDown','Escape'])assert.deepEqual(press(code,{[option]:true}).actions,[]);
console.log('Piano-roll shortcuts yield to menu and dialog focus PASS');
