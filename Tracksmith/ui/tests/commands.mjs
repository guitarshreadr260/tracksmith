import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import ts from 'typescript';
import {createCommandRegistry,menuNames} from '../src/commands.ts';
let calls=0;
const registry=createCommandRegistry([{id:'test',menu:'File',label:'Test',enabled:false,run:()=>calls++}]);
assert.equal(registry.execute('test'),false);assert.equal(registry.execute('missing'),false);assert.equal(calls,0);
registry.commands[0].enabled=true;assert.equal(registry.execute('test'),true);assert.equal(calls,1);
assert.throws(()=>createCommandRegistry([registry.commands[0],registry.commands[0]]),/Duplicate/);
const source=readFileSync(new URL('../src/Studio.tsx',import.meta.url),'utf8');
const block=source.slice(source.indexOf(' const available='),source.indexOf(' return <main'));
const js=ts.transpileModule(block+';return registry;', {compilerOptions:{target:ts.ScriptTarget.ES2022}}).outputText;
function fixture(overrides={}){
 const sent=[],changes=[];
 const context={tool:'smart',moveInsertion:()=>{},allTrack:null,editCursor:null,editFocus:'timeline',pianoContext:undefined,disabled:false,audio:{recording:false,seconds:2,canMidiUndo:true},chosen:{id:3,kind:'MIDI'},selection:{id:8,track:3,midi:true},selected:3,tracks:[{}],rangeStart:0,rangeEnd:4,rangeTrack:null,view:'edit',clips:[{id:5,track:3,start:1,length:2,offset:0,sourceDuration:8}],elasticOpen:false,setElasticOpen:()=>{},gridStep:.25,native:true,bpm:120,play:()=>sent.push({type:'play'}),record:()=>sent.push({type:'record'}),editor:false,playlist:false,zoom:1,snap:true,command:c=>sent.push(c),addInstrument:()=>changes.push('add'),createCommandRegistry,...overrides};
 for(const key of ['setTool','setAllTrack','setEditFocus','setLauncher','setNewSetup','setSelection','setRangeTrack','setView','setLibraryOpen','setEditor','setZoom','setPlaylist','setSnap','setIOOpen','setDetails','setNotice'])context[key]=v=>changes.push([key,v]);
 return {registry:new Function(...Object.keys(context),js)(...Object.values(context)),sent,changes};
}
const midi=fixture();assert.deepEqual([...new Set(midi.registry.commands.map(c=>c.menu))],menuNames);
assert.equal(midi.registry.execute('audio.import'),false);assert.equal(midi.registry.execute('clip.split'),false);
assert.equal(midi.registry.execute('edit.undo'),true);assert.equal(midi.registry.execute('midi.import'),true);
assert.deepEqual(midi.sent,[{type:'undo',track:3},{type:'midiImport',track:3}]);
const clip=fixture({chosen:{id:3,kind:'AUDIO'},selection:{id:5,track:3}});clip.registry.execute('clip.split');clip.registry.execute('audio.import');
assert.deepEqual(clip.sent,[{type:'clipEdit',id:5,track:3,action:'split',position:2},{type:'import',id:3}]);
const busy=fixture({disabled:true});for(const id of ['file.new','file.open','file.save','track.add','midi.import','edit.delete'])assert.equal(busy.registry.execute(id),false);
const range=fixture({selection:null,rangeTrack:3});assert.equal(range.registry.execute('edit.delete'),true);assert.deepEqual(range.sent,[{type:'clipboard',action:'clear',track:3,ids:[],start:0,end:4}]);
console.log('Command registry: menu coverage, availability, dispatch, MIDI undo, clip split, range deletion and duplicate IDs PASS');

const key=(code,extra={})=>({code,ctrlKey:false,metaKey:false,altKey:false,shiftKey:false,repeat:false,...extra});
const zoom=fixture();assert.equal(zoom.registry.shortcut(key('KeyT')),true);assert.equal(zoom.changes.at(-1)[1](1),1.25);
zoom.registry.shortcut(key('KeyR'));assert.equal(zoom.changes.at(-1)[1](1),.8);
zoom.registry.shortcut(key('KeyT',{repeat:true}));assert.equal(zoom.changes.at(-1)[1](63),64);
for(const modifier of ['ctrlKey','metaKey','altKey','shiftKey'])assert.equal(zoom.registry.shortcut(key('KeyR',{[modifier]:true})),false);
const minimum=fixture({zoom:.5});minimum.registry.shortcut(key('KeyR'));assert.equal(minimum.changes.length,0);
const maximum=fixture({zoom:64});maximum.registry.shortcut(key('KeyT'));assert.equal(maximum.changes.length,0);
const mix=fixture({view:'mix'});mix.registry.shortcut(key('KeyT'));assert.equal(mix.changes.length,0);
const file=fixture();assert.equal(file.registry.shortcut(key('KeyS')),false);file.registry.shortcut(key('KeyS',{ctrlKey:true}));assert.deepEqual(file.sent,[{type:'saveSession'}]);
file.registry.shortcut(key('KeyS',{ctrlKey:true,repeat:true}));assert.equal(file.sent.length,1);
console.log('R/T zoom direction, repeat, bounds, mixer guard, modifiers, Ctrl-only save and repeat suppression PASS');

const duplicate=fixture({selection:{id:5,track:3}});duplicate.registry.shortcut(key('KeyD',{ctrlKey:true}));assert.deepEqual(duplicate.sent,[{type:'clipboard',track:3,ids:[5],action:'duplicate'}]);
const mixer=fixture();assert.equal(mixer.registry.shortcut(key('Equal',{ctrlKey:true})),true);assert.equal(mixer.registry.shortcut(key('KeyM',{ctrlKey:true,shiftKey:true})),true);
console.log('Build 18 duplicate and non-conflicting mixer shortcuts PASS');

for(const [code,action] of [['KeyX','cut'],['KeyC','copy'],['KeyB','clear']]){const test=fixture();test.registry.shortcut(key(code,{ctrlKey:true}));assert.deepEqual(test.sent,[{type:'clipboard',track:3,ids:[8],action}]);}
const paste=fixture({audio:{recording:false,seconds:2,clipboardKind:'midi'},editCursor:{track:3,seconds:4}});paste.registry.shortcut(key('KeyV',{ctrlKey:true}));assert.deepEqual(paste.sent,[{type:'clipboard',action:'paste',track:3,position:4}]);
const select=fixture();select.registry.shortcut(key('KeyA',{ctrlKey:true}));assert.ok(select.changes.some(c=>c[0]==='setAllTrack'&&c[1]===3));
const wrongPaste=fixture({audio:{recording:false,seconds:2,clipboardKind:'audio'}});assert.equal(wrongPaste.registry.execute('edit.paste'),false);
console.log('Clipboard shortcuts, edit-cursor paste, select all, clear and compatible-track guards PASS');

const finishing=fixture({disabled:true,audio:{loading:true,recordingBusy:true,recording:false,playing:true}});assert.equal(finishing.registry.execute('transport.stop'),true);assert.deepEqual(finishing.sent,[{type:'stop'}]);
console.log('Stop remains available during recording finalization PASS');
