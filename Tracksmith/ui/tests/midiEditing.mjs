import assert from 'node:assert/strict';
import {dragNote,noteName,marqueeNotes,transformNotes,noteViewTop} from '../src/midiEditing.ts';
const n={track:2,id:7,pitch:60,channel:10,start:2,length:1,velocity:.72};
assert.equal(noteName(60),'C4');
assert.deepEqual(dragNote(n,'move',.38,2,.25,true),{...n,start:2.5,pitch:62});
assert.equal(dragNote(n,'move',-20,-200,.25,true).start,0);
assert.equal(dragNote(n,'move',0,200,.25,true).pitch,127);
assert.equal(dragNote(n,'resize',-20,0,.25,true).length,.25);
assert.equal(dragNote(n,'resize',.37,0,.25,true).length,1.25);
assert.equal(dragNote(n,'move',.13,0,.25,false).start,2.13);
assert.equal(dragNote({...n,start:99998},'resize',20,0,.25,true).length,2);
console.log('Piano roll movement, pitch limits, snapping, resize bounds and MIDI properties: PASS');

const chord=[n,{...n,id:8,start:3,pitch:64,length:2}];
assert.deepEqual(marqueeNotes(chord,2.5,3.5,60,64),[7,8]);
assert.deepEqual(marqueeNotes(chord,3.5,2.5,61,65),[8]);
assert.deepEqual(marqueeNotes(chord,10,11,0,127),[]);
assert.deepEqual(transformNotes(chord,-9,-99).map(n=>[n.start,n.pitch]),[[0,0],[1,4]]);
assert.deepEqual(transformNotes(chord,0,99).map(n=>n.pitch),[123,127]);
assert.deepEqual(transformNotes(chord,0,12).map(n=>n.pitch),[72,76]);
assert.deepEqual(transformNotes(chord,.5,12,true).map(n=>[n.start,n.pitch,n.length]),[[2,60,1.5],[3,64,2.5]]);
assert.equal(transformNotes(chord,-100,0,true)[0].length,.010000000000000009);
console.log('Marquee overlap, reversed drags, group transposition, boundary clamping and group resize: PASS');

assert.equal(noteViewTop([]),(127-66)*18);
const drums=[36,38,42,42,42,49,84].map((pitch,i)=>({...n,id:i+1,pitch}));
assert.equal(noteViewTop(drums),(127-48)*18); // Imported drum register, not the highest cymbal.
assert.ok(noteViewTop(drums)>noteViewTop([{...n,pitch:72}]));
console.log('Imported drum-note register centers around the median hit pitch: PASS');
